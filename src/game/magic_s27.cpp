// Three spell overlays compiled into the exe, round nine group S27
// (docs/magic_s27.md): the PSX's MAGIC118, MAGIC120 and MAGIC121.EMI,
// Magic_Rows rows 60, 16 and 122. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Burn, Whelp Breath and DragonBreath; the
// names below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC118 0x4DA220..0x4DACCD: the effect at the source sprite, eight
//     flames (a kind-1 task each, 0x2F) rising in a ring of textured quads, a
//     ring and a disc of gouraud primitives at the actor, a CLUT row made
//     semi-transparent;
//   - MAGIC120 0x4DACD0..0x4DC109: a beam (kind-1 task 0x0F, +1 0) from the
//     caster's screen point toward the targets' centre, a band of textured
//     quads; sprites of the same kind (+1 1) - two at the caster, then flames
//     over every live enemy and a glow under each;
//   - MAGIC121 0x4DC110..0x4DD8A4: a beam (kind-1 task 0x5A) of four gouraud
//     quads a step, with six crackling lines along it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and
// WhelpBreathBeam_Draw aborts where the original divides by a zero +0xB.
#include "game/magic_s27.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s27_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace raw = magic_s27::raw;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap its +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, std::uint32_t v) { SetWord(Mem(kS + k), v & 0xFFFF); }
void AddSW(unsigned k, std::uint32_t v) { SetSW(k, SW(k) + v); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, std::uint32_t v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl eax, n` then `sar eax, 0xC`.
int Shl12(int a, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* EnemyRecord(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }
void SetPtr(unsigned char* at, const void* p) {
    SetLong(at, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}

int RandCall() { return MH_CALL(Rand)(); }
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
int SinKAdd(int a, unsigned k, unsigned add) {
    const int v = SinK(a, k);
    return v + SS(add);
}
int CosKAdd(int a, unsigned k, unsigned add) {
    const int v = CosK(a, k);
    return v + SS(add);
}
// (sin << n) >> 12 plus a scratch word, 16 bits.
std::uint32_t SinShlW(int a, unsigned n, unsigned add) {
    const int v = Sin(a);
    return static_cast<std::uint32_t>(Shl12(v, n)) + SW(add);
}
std::uint32_t CosShlW(int a, unsigned n, unsigned add) {
    const int v = Cos(a);
    return static_cast<std::uint32_t>(Shl12(v, n)) + SW(add);
}

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn1 = void (__cdecl*)(unsigned);
using PrimFn = void (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call1(std::uint32_t address, unsigned a) { MH_AT(Fn1, address)(a); }

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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

}  // namespace

#define S27_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC118 (row 60, Burn read one id down)

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
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
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
    SetLong(Sc() + 0x14, 0);
    SetLong(Sc() + 0x20, 8);
    Sc()[0xA] = 2;
    Sc()[0xB] = 0x10;
    Sc()[9] = 0x20;
    Inc(Sc()[1]);
}

namespace {
// The flame's climb: its speed +0x20 up by +0xA, its height +0x14 up by the
// speed (dwords).
void Climb() {
    unsigned char* const s = Sc();
    SetLong(s + 0x20, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x20)) + s[0xA]));
    SetLong(s + 0x14, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x14)) +
                                                static_cast<std::uint32_t>(Long(s + 0x20))));
}
}  // namespace

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
        const std::uint32_t x = static_cast<std::uint32_t>(Long(s + 0x34)) + shift;
        const std::uint32_t z = static_cast<std::uint32_t>(Long(s + 0x38)) + shift;
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
        SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
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

// ===========================================================================
// MAGIC120 (row 16, Whelp Breath) and MAGIC121 (row 122, DragonBreath)

namespace {

// The beams' aim, both spells (WhelpBreathBeam_Aim, DragonBreathBeam_Aim once
// +9 runs out): the owner's direction byte; the task to the targets' centre
// (MagicFx_CenterOnSide) and its screen point kept in DamageScratch +0xC /
// +0xE; MagicFx_FormationOffset; the task back at its owner, its screen point
// moved by the words +0xC / +0x10 into +8 / +0xA; +0x14 the angle
// Math_Ratan2(kept x - x, kept y - y) from there to the centre.
void Aim() {
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, Word(Sc() + 0x2E));
    SetSW(0xE, Word(Sc() + 0x30));
    MH_CALL(MagicFx_FormationOffset)();
    AtOwner();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + Word(Sc() + 0xC));
    SetWord(Sc() + 0x30, Word(Sc() + 0x30) + Word(Sc() + 0x10));
    const unsigned char* const s = Sc();
    const short x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(8, static_cast<std::uint16_t>(x));
    SetSW(0xA, static_cast<std::uint16_t>(y));
    // Math_Ratan2(y, x) as the original pushes it: first the kept x less x,
    // then the kept y less y.
    const float first = static_cast<float>(SS(0xC) - x);
    const float second = static_cast<float>(SS(0xE) - y);
    const int angle = MH_CALL(Math_Ratan2)(first, second);
    SetLong(Sc() + 0x14, angle);
}

}  // namespace

// original 0x4DACD0: the kind-2 task. A three-entry stack table by +1 -
// WhelpBreath_Start, WhelpBreath_WaitBeam, Leech_WaitOrbs (group S17's: held
// until +0xB is 0xFF, then the done flag and free).
S27_EXPORT void __cdecl WhelpBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::WhelpBreath_Start, bof3::addr::WhelpBreath_WaitBeam,
                                                 bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("WhelpBreath_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DAD00: the owner's direction byte and position; three children
// of kind 1, parameter 0x0F: the beam (+1 0, +9 4, this task its +0x80), a
// sprite at the owner (+1 1, +0xB 0) and, unless direction bit 1, another (+1
// 1, +0xB 1); CLUT row 26's three runs, the first and third semi-transparent;
// +0xB and +9 0, +1 on.
S27_EXPORT void __cdecl WhelpBreath_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    {
        const unsigned slot = NewTask(0xF);
        unsigned char* const self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 0;
        c[9] = 4;
    }
    {
        const unsigned slot = NewTask(0xF);
        unsigned char* const owner = Owner();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, owner);
        c[1] = 1;
        c[0xB] = 0;
    }
    if ((Sc()[8] & 2) == 0) {
        const unsigned slot = NewTask(0xF);
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, Owner());
        c[1] = 1;
        c[0xB] = 1;
    }
    static constexpr bool kStp[3] = {true, false, true};
    LightClutRow(3, kStp);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DAE80: held until +0xB is 1 (the beam grown); then two more
// sprites (kind 1, 0x0F, +1 1, this task their +0x80) with +0xB 2 and 3; +1
// on.
S27_EXPORT void __cdecl WhelpBreath_WaitBeam(void) {
    if (Sc()[0xB] != 1) return;
    unsigned char* self = nullptr;
    for (unsigned kind = 2; kind <= 3; ++kind) {
        const unsigned slot = NewTask(0xF);
        self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 1;
        c[0xB] = static_cast<unsigned char>(kind);
    }
    Inc(self[1]);
}

// original 0x4DAF00: the child's kind-1 task, a jmp through
// WhelpBreathChild_Kinds (two entries) by +1: the beam, the sprites.
S27_EXPORT void __cdecl WhelpBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::WhelpBreathBeam_Run, bof3::addr::WhelpBreathSprite_Run};
    const unsigned kind = Sc()[1];
    if (kind >= 2) PastTable("WhelpBreathChild_Task", kind, 2);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4DAF20: a call through WhelpBreathBeam_Phases (five entries) by
// +2 - Aim, Grow, then MAGIC122's sweep, hold and fade - then, unless the
// owner's +0xB is 0xFF or +2 is 0, the beam (a tail jmp).
S27_EXPORT void __cdecl WhelpBreathBeam_Run(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::WhelpBreathBeam_Aim, bof3::addr::WhelpBreathBeam_Grow,
                                                 bof3::addr::BreathBeam_Hit, bof3::addr::BreathBeam_Hold,
                                                 bof3::addr::BreathBeam_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("WhelpBreathBeam_Run", phase, 5);
    magic_harness::Phase(kPhases[phase])();
    if (Owner()[0xB] == 0xFF) return;
    if (Sc()[2] == 0) return;
    Call0(bof3::addr::WhelpBreathBeam_Draw);
}

// original 0x4DAF50: +9 down; at 0 the aim (above); +0xB 0x11, +9 0, +0xA 1,
// +4 0x10, +2 on; sound 0x100.
S27_EXPORT void __cdecl WhelpBreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Aim();
    Sc()[0xB] = 0x11;
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Sc()[4] = 0x10;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4DB090: +0xA up by three (the beam's length), +9 up; at 8 the
// owner's +0xB 1 (WhelpBreath_WaitBeam's signal) and +2 on.
S27_EXPORT void __cdecl WhelpBreathBeam_Grow(void) {
    AddB(Sc()[0xA], 3);
    Inc(Sc()[9]);
    if (Sc()[9] != 8) return;
    Owner()[0xB] = 1;
    Inc(Sc()[2]);
}

namespace {

// One step's width of the Whelp beam: DamageScratch +0 = (sin(angle) x step x
// 2 >> 12) + base, divided by +0xB (signed, the original's idiv), plus 1.
void BeamWidth(std::uint32_t angle, int step, int base) {
    const int v = Sin(static_cast<int>(angle));
    const int r = static_cast<int>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(step) * 2u) >> 12;
    const int width = r + base;
    SetSW(0, static_cast<std::uint32_t>(width));
    const unsigned divisor = Sc()[0xB];
    if (divisor == 0) bof3::Fatal("WhelpBreathBeam_Draw: +0xB is 0 - the original divides by zero here");
    SetSW(0, static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(width)) / static_cast<int>(divisor) + 1));
}
// The four texture v bytes of a step: (+9 x 30 + step) x 8 on the near edge,
// one more on the far.
void BeamV(unsigned char* p, unsigned step) {
    const unsigned char* const s = Sc();
    const auto v0 = static_cast<unsigned char>((s[9] * 0x1E + step) << 3);
    const auto v1 = static_cast<unsigned char>((s[9] * 0x1E + step + 1) << 3);
    p[0x15] = v0;
    p[0x29] = v0;
    p[0x3D] = v1;
    p[0x51] = v1;
}
// The shades of a step's quad: +4 x 12 on the centre line, +4 on the edge;
// `centre_first` says which pair of points is the centre line.
void BeamShade(unsigned char* p, bool centre_first) {
    static constexpr unsigned kFirst[6] = {4, 5, 6, 0x2C, 0x2D, 0x2E};      // points 0 and 2
    static constexpr unsigned kSecond[6] = {0x18, 0x19, 0x1A, 0x40, 0x41, 0x42};   // points 1 and 3
    const unsigned char* const s = Sc();
    const unsigned* const centre = centre_first ? kFirst : kSecond;
    const unsigned* const edge = centre_first ? kSecond : kFirst;
    SetSW(6, (s[4] * 12u) & 0xFFFF);
    for (unsigned i = 0; i < 6; ++i) p[centre[i]] = SB(6);
    SetSW(6, s[4]);
    for (unsigned i = 0; i < 6; ++i) p[edge[i]] = SB(6);
}

}  // namespace

// original 0x4DB0D0: the beam - +0xA steps from the task's screen point along
// an angle that wavers about +0x14 (a sine of (+9 + step) x 0x40), 16 units a
// step; each step two semi-transparent POLY_GT4 either side of the line, of
// width (sin x step x 2 >> 12 + step x 4) / +0xB + 1 at the near end and the
// same for step + 1 at the far; page (0x380, 0x100), CLUT (0, 0x1FA). Tpage
// 0x35 for the beam, 0x15 after it, all committed to slot 3. Screen space:
// the points are written as floats, no projection.
S27_EXPORT void __cdecl WhelpBreathBeam_Draw(void) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, Word(s + 0x2E));
        SetSW(0xA, Word(s + 0x30));
        SetSW(2, Word(s + 0x14));
    }
    for (int k = 1; k < Sc()[0xA] + 1; ++k) {
        SetSW(0xC, SW(8));
        SetSW(0xE, SW(0xA));
        {
            const std::uint32_t arg = ((static_cast<std::uint32_t>(Sc()[9] + k) << 2) & 0xFF) << 4;
            const int v = Sin(static_cast<int>(arg));
            const int t = static_cast<int>(static_cast<std::uint32_t>(v) * 11u << 5) >> 12;
            const std::uint32_t angle = (static_cast<std::uint32_t>(t) + SW(2)) & 0xFFF;
            SetSW(4, angle);
            const int s1 = Sin(static_cast<int>(angle));
            AddSW(8, static_cast<std::uint32_t>(Shl12(s1, 4)));
            const int c1 = Cos(SS(4));
            AddSW(0xA, static_cast<std::uint32_t>(Shl12(c1, 4)));
        }
        const unsigned b9 = Sc()[9];
        const std::uint32_t a0 = ((static_cast<std::uint32_t>(k) - b9) & 0x3F) << 6;
        const std::uint32_t a1 = ((static_cast<std::uint32_t>(k) - b9 - 1) & 0x3F) << 6;
        // the first quad: the centre line (+0xC, +0xE) .. (+8, +0xA) and the
        // edge a quarter turn one way
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (SW(2) + 0x400u) & 0xFFF);
        BeamWidth(a1, k - 1, k * 4);
        PutFloat(p + 0x1C, SinKAdd(SS(4), 0, 0xC));
        PutFloat(p + 0x20, CosKAdd(SS(4), 0, 0xE));
        BeamWidth(a0, k, k * 4 + 4);
        PutFloat(p + 0x44, SinKAdd(SS(4), 0, 8));
        PutFloat(p + 0x48, CosKAdd(SS(4), 0, 0xA));
        PutFloat(p + 8, SS(0xC));
        PutFloat(p + 0xC, SS(0xE));
        PutFloat(p + 0x30, SS(8));
        PutFloat(p + 0x34, SS(0xA));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
        p[0x14] = 0x40;
        p[0x28] = 0x80;
        p[0x3C] = 0x40;
        p[0x50] = 0x80;
        BeamV(p, static_cast<unsigned>(k));
        BeamShade(p, true);
        Commit(3, 0x54);
        // the second: the edge the other way, the centre line last
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (SW(2) - 0x400u) & 0xFFF);
        BeamWidth(a1, k - 1, k * 4);
        PutFloat(p + 8, SinKAdd(SS(4), 0, 0xC));
        PutFloat(p + 0xC, CosKAdd(SS(4), 0, 0xE));
        BeamWidth(a0, k, k * 4 + 4);
        PutFloat(p + 0x30, SinKAdd(SS(4), 0, 8));
        PutFloat(p + 0x34, CosKAdd(SS(4), 0, 0xA));
        PutFloat(p + 0x1C, SS(0xC));
        PutFloat(p + 0x20, SS(0xE));
        PutFloat(p + 0x44, SS(8));
        PutFloat(p + 0x48, SS(0xA));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
        p[0x14] = 0;
        p[0x28] = 0x40;
        p[0x3C] = 0;
        p[0x50] = 0x40;
        BeamV(p, static_cast<unsigned>(k));
        BeamShade(p, false);
        Commit(3, 0x54);
    }
    ModeCommit(3, 0x15);
}

// original 0x4DB7A0: the sprite child's step: a draw mode (tpage 0x15, slot
// 3); the sprite bank 0x9039D8 switched to the effects' (0x8E3580) around a
// call through WhelpBreathSprite_Phases (eight entries) by +2, then back
// (0x8B3580).
S27_EXPORT void __cdecl WhelpBreathSprite_Run(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::WhelpBreathSprite_Start, bof3::addr::WhelpBreathSprite_Play, bof3::addr::WhelpBreathSprite_Hold,
        bof3::addr::WhelpBreathFlames_Grow,  bof3::addr::WhelpBreathFlames_End, bof3::addr::WhelpBreathGlow_Grow,
        bof3::addr::WhelpBreathGlow_Hold,    bof3::addr::WhelpBreathGlow_End};
    ModeCommit(3, 0x15);
    SetLong(Mem(raw::kSpriteBank), static_cast<std::int32_t>(raw::kBankEffects));
    const unsigned phase = Sc()[2];
    if (phase >= 8) PastTable("WhelpBreathSprite_Run", phase, 8);
    magic_harness::Phase(kPhases[phase])();
    SetLong(Mem(raw::kSpriteBank), static_cast<std::int32_t>(raw::kBankDefault));
}

// original 0x4DB7F0: the sprite at its owner (direction byte, position);
// sprite fields +0x24 4, +0x25 0x1D, +0x26 0, +0x27 0xA1, +0x28 0, +0x29 3,
// +0x2A the owner's direction bit 0, +0x2B 0, word +0x2C 0; then by +0xB
// (a jump table, 0..3; any other nothing):
//   0: +2 on; with direction bit 1 +0x29 4, +0x2A its bit 0 inverted and
//      animation 3, else animation 0;
//   1: +9 0x78, +2 2, animation 1;
//   2: WhelpBreathFlames_Ages cleared, +9 0, +2 3 (the flames);
//   3: +9 0, +0xA 0x50, +2 5 (the glow).
S27_EXPORT void __cdecl WhelpBreathSprite_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0xA1;
    s[0x28] = 0;
    s[0x2A] = static_cast<unsigned char>(Owner()[8] & 1);
    s[0x29] = 3;
    s[0x24] = 4;
    SetWord(s + 0x2C, 0);
    s[0x2B] = 0;
    switch (s[0xB]) {
    case 0:
        Inc(s[2]);
        if (Sc()[8] & 2) {
            unsigned char* const t = Sc();
            t[0x29] = 4;
            t[0x2A] = static_cast<unsigned char>(~t[8] & 1);
            MH_CALL(Sprite_SetAnimation)(3);
        } else {
            MH_CALL(Sprite_SetAnimation)(0);
        }
        break;
    case 1:
        s[9] = 0x78;
        Sc()[2] = 2;
        MH_CALL(Sprite_SetAnimation)(1);
        break;
    case 2:
        for (unsigned i = 0; i < 6; ++i) WhelpBreathFlames_Ages[i] = 0;
        s[9] = 0;
        Sc()[2] = 3;
        break;
    case 3:
        s[9] = 0;
        Sc()[0xA] = 0x50;
        Sc()[2] = 5;
        break;
    default: break;
    }
}

// original 0x4DB950: the sprite's screen point; its script ticked once; at its
// end the task freed.
S27_EXPORT void __cdecl WhelpBreathSprite_Play(void) {
    MH_CALL(Sprite_UpdateScreen)();
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) != 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DB970: the script ticked, the screen point; +9 down; at 0 the
// task freed.
S27_EXPORT void __cdecl WhelpBreathSprite_Hold(void) {
    MH_CALL(Sprite_ScriptTick)();
    MH_CALL(Sprite_UpdateScreen)();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DB9A0: the flames; +9 up; at 0x60 +2 on.
S27_EXPORT void __cdecl WhelpBreathFlames_Grow(void) {
    Call0(bof3::addr::WhelpBreathFlames_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x60) Inc(Sc()[2]);
}

// original 0x4DB9D0: the flames; +9 up; at 0x70 the task freed.
S27_EXPORT void __cdecl WhelpBreathFlames_End(void) {
    Call0(bof3::addr::WhelpBreathFlames_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x70) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DBA00: the glow; +9 up; at 0x10 +2 on.
S27_EXPORT void __cdecl WhelpBreathGlow_Grow(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4DBA30: the glow; +0xA down; at 0 +2 on.
S27_EXPORT void __cdecl WhelpBreathGlow_Hold(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4DBA60: the glow; +9 down; at 0 the task freed.
S27_EXPORT void __cdecl WhelpBreathGlow_End(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DBA90: a draw mode (tpage 0x35, slot 3); for each of the three
// offsets of WhelpBreath_FlameOffsets (dx, dz, dy dwords) and each live enemy:
// +0xB the offset's index, +0xA the enemy's, the pair turned by direction
// (0x446770), the task at the enemy plus the offset, the enemy's direction
// byte, and a column of flames under the task's matrix. Then the six ages of
// WhelpBreathFlames_Ages: each i with 3i below +9 up by one, and in phase 3
// each wrapped to 0..15.
S27_EXPORT void __cdecl WhelpBreathFlames_Draw(void) {
    ModeCommit(3, 0x35);
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const offset = reinterpret_cast<const unsigned char*>(WhelpBreath_FlameOffsets) + 0xC * i;
        for (unsigned e = 0; e < 8; ++e) {
            if ((MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) != 0) continue;
            const unsigned char* const r = EnemyRecord(e);
            unsigned char* s = Sc();
            s[0xB] = static_cast<unsigned char>(i);
            s[0xA] = static_cast<unsigned char>(e);
            SetLong(s + 0xC, Long(offset));
            SetLong(s + 0x10, Long(offset + 4));
            MH_AT(PrimFn, raw::kTurnOffset)(s);
            s = Sc();
            SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(r + 0x34)) +
                                                        static_cast<std::uint32_t>(Long(s + 0xC))));
            SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x10)) +
                                                        static_cast<std::uint32_t>(Long(r + 0x38))));
            SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(offset + 8)) +
                                                        static_cast<std::uint32_t>(Long(r + 0x3C))));
            s[8] = r[8];
            Call0(bof3::addr::WhelpBreathFlames_PushMatrix);
            Call0(bof3::addr::WhelpBreathFlames_DrawColumn);
            MH_CALL(Gte_PopMatrix)();
        }
    }
    const unsigned char* const s = Sc();
    for (unsigned j = 0; j < 6; ++j) {
        if (3 * j < s[9]) Inc(WhelpBreathFlames_Ages[j]);
        if (s[2] == 3) WhelpBreathFlames_Ages[j] &= 0xF;
    }
}

// original 0x4DBBC0: up to six flames stacked in a column, flame j drawn while
// 3j is below +9 and its age WhelpBreathFlames_Ages[j] below 0x10: a
// semi-transparent POLY_FT4 square turned 45 degrees, half-diagonal age x 4 +
// 0x32, at height age x 15, fading (0x61 - age x 6); page (0x340, 0x100),
// CLUT (0x20, 0x1FA), u 8..0x28, v 0x40..0x60. Slot 3.
S27_EXPORT void __cdecl WhelpBreathFlames_DrawColumn(void) {
    for (unsigned j = 0, k = 0; k < 0x12; k += 3, ++j) {
        if (static_cast<int>(k) >= static_cast<int>(Sc()[9])) continue;
        if (WhelpBreathFlames_Ages[j] >= 0x10) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned age = WhelpBreathFlames_Ages[j];
        SetSW(0, age * 4 + 0x32);
        const std::uint32_t height = age * 15;
        static constexpr int kCorners[4] = {0x200, 0x600, 0xE00, 0xA00};
        for (unsigned c = 0; c < 4; ++c) {
            SetVW(8 * c, static_cast<std::uint32_t>(CosK(kCorners[c], 0)));
            SetVW(8 * c + 2, static_cast<std::uint32_t>(SinK(kCorners[c], 0)));
            SetVW(8 * c + 4, height);
        }
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
        p[0x14] = 8;
        p[0x15] = 0x40;
        p[0x24] = 0x28;
        p[0x25] = 0x40;
        p[0x34] = 8;
        p[0x35] = 0x60;
        p[0x44] = 0x28;
        p[0x45] = 0x60;
        SetSW(6, 0x61u - WhelpBreathFlames_Ages[j] * 6u);
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        Commit(3, 0x48);
    }
}

// original 0x4DBE10: the task's matrix pushed (MagicFx_PushActorMatrix's
// shape): Camera_Matrix x the rotation by direction (+8 0: y 0xC00, 1: x
// 0x400, 2: y 0x400, 3: x 0xC00, else none), translation RotTrans of (x >> 9
// - 0x4000, z >> 9 - 0x4000, -(height / 2)).
S27_EXPORT void __cdecl WhelpBreathFlames_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short rot[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    switch (s[8]) {
    case 0: rot[1] = 0xC00; break;
    case 1: rot[0] = 0x400; break;
    case 2: rot[1] = 0x400; break;
    case 3: rot[0] = 0xC00; break;
    default: break;
    }
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S27_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4DBF00: for each live enemy, the task at it (direction byte, x,
// z, the height word +0x3E) and the glow under the actor's matrix.
S27_EXPORT void __cdecl WhelpBreathGlow_Draw(void) {
    for (unsigned e = 0; e < 8; ++e) {
        if ((MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) != 0) continue;
        const unsigned char* const r = EnemyRecord(e);
        unsigned char* const s = Sc();
        s[8] = r[8];
        SetLong(s + 0x34, Long(r + 0x34));
        SetLong(s + 0x38, Long(r + 0x38));
        SetWord(s + 0x3E, Word(r + 0x3E));
        MH_CALL(MagicFx_PushActorMatrix)();
        Call0(bof3::addr::WhelpBreathGlow_DrawFan);
        MH_CALL(Gte_PopMatrix)();
    }
}

// original 0x4DBF70: one semi-transparent POLY_G3 from the actor matrix's
// origin to two points at radius 0x100 + (frame & 0x3F), either side (6 x 0x40)
// of the angle WhelpBreath_GlowAngles[+8] x 0x40 (the direction byte
// unbounded, as in the original); the origin +9 x 10 + 1, the rim 1. Tpage
// 0xD5, slot 5.
S27_EXPORT void __cdecl WhelpBreathGlow_DrawFan(void) {
    SetSW(0, (Frame_Counter & 0x3F) + 0x100);
    ModeCommit(5, 0xD5);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetVW(0, 0);
    SetVW(2, 0);
    SetVW(4, 0);
    {
        const int a = static_cast<int>(((WhelpBreath_GlowAngles[Sc()[8]] - 6u) & 0x3F) << 6);
        SetVW(8, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0xA, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0xC, 0);
    }
    {
        const int a = static_cast<int>(((WhelpBreath_GlowAngles[Sc()[8]] + 6u) & 0x3F) << 6);
        SetVW(0x10, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0x12, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0x14, 0);
    }
    const auto centre = static_cast<unsigned char>(Sc()[9] * 10 + 1);
    p[4] = centre;
    p[5] = centre;
    p[6] = centre;
    for (unsigned i : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[i] = 1;
    Rtp3(p);
    MH_CALL(Gte_PrimDepths3_10B)(p);
    Commit(5, 0x34);
}

// ---------------------------------------------------------------------------
// MAGIC121 (row 122, DragonBreath)

// original 0x4DC110: the kind-2 task. A two-entry stack table by +1 -
// DragonBreath_Start, Leech_WaitOrbs (held until +0xB is 0xFF).
S27_EXPORT void __cdecl DragonBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::DragonBreath_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("DragonBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DC140: the owner's direction byte and position; the beam (kind
// 1, 0x5A: +1 0, +9 4, this task its +0x80); CLUT row 26's three runs, the
// first and third semi-transparent; sound 0x100; +0xB and +9 0, +1 on.
S27_EXPORT void __cdecl DragonBreath_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    {
        const unsigned slot = NewTask(0x5A);
        unsigned char* const self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 0;
        c[9] = 4;
    }
    static constexpr bool kStp[3] = {true, false, true};
    LightClutRow(3, kStp);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DC260: the beam's kind-1 task, a jmp through
// DragonBreathChild_Kinds (one entry) by +1.
S27_EXPORT void __cdecl DragonBreathBeam_Task(void) {
    const unsigned kind = Sc()[1];
    if (kind >= 1) PastTable("DragonBreathBeam_Task", kind, 1);
    magic_harness::Phase(bof3::addr::DragonBreathBeam_Run)();
}

// original 0x4DC280: a call through DragonBreathBeam_Phases (seven entries) by
// +2; then while +0 and +2 are set: every eighth frame three new jitters
// (+0x5D Rand & 1, +0x5E and +0x5F Rand & 3), six crackling lines - three on
// one side (lengths +0x5D + 3, +0x5E + 7, 0xB - +0x5F steps), three on the
// other (4 - +0x5D, +0x5F + 8, 0xC - +0x5E) - and the beam (a tail jmp).
S27_EXPORT void __cdecl DragonBreathBeam_Run(void) {
    static constexpr std::uint32_t kPhases[7] = {
        bof3::addr::DragonBreathBeam_Aim,      bof3::addr::DragonBreathBeam_Widen, bof3::addr::DragonBreathBeam_Hit,
        bof3::addr::DragonBreathBeam_Brighten, bof3::addr::DragonBreathBeam_Hold,  bof3::addr::DragonBreathBeam_Dim,
        bof3::addr::DragonBreathBeam_End};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("DragonBreathBeam_Run", phase, 7);
    magic_harness::Phase(kPhases[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    if ((Frame_Counter & 7) == 0) {
        Sc()[0x5D] = static_cast<unsigned char>(RandCall() & 1);
        Sc()[0x5E] = static_cast<unsigned char>(RandCall() & 3);
        Sc()[0x5F] = static_cast<unsigned char>(RandCall() & 3);
    }
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(Sc()[0x5D] + 3));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(Sc()[0x5E] + 7));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(0xB - Sc()[0x5F]));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(4 - Sc()[0x5D]));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(Sc()[0x5F] + 8));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(0xC - Sc()[0x5E]));
    Call0(bof3::addr::DragonBreathBeam_Draw);
}

// original 0x4DC360: +9 down; at 0 the aim (as WhelpBreathBeam_Aim's), the
// three jitters, dword +0xC 0, +4 0, +0xB 0, +9 0x10, +0xA 0, +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Aim();
    Sc()[0x5D] = static_cast<unsigned char>(RandCall() & 1);
    Sc()[0x5E] = static_cast<unsigned char>(RandCall() & 3);
    Sc()[0x5F] = static_cast<unsigned char>(RandCall() & 3);
    SetLong(Sc() + 0xC, 0);
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4DC4D0: +0xA (the length) up by 8; at 0x20 +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Widen(void) {
    AddB(Sc()[0xA], 8);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4DC4F0: +0xB (the width) up; at 8 the target flags 0x10 and +2
// on.
S27_EXPORT void __cdecl DragonBreathBeam_Hit(void) {
    Inc(Sc()[0xB]);
    if (Sc()[0xB] != 8) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[2]);
}

// original 0x4DC530: +4 up by two; at 0x10 +2 on (a body seven overlays
// reach).
S27_EXPORT void __cdecl DragonBreathBeam_Brighten(void) {
    AddB(Sc()[4], 2);
    if (Sc()[4] == 0x10) Inc(Sc()[2]);
}

// original 0x4DC550: dword +0xC up; at 0x2E +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Hold(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0xC)) + 1));
    if (Long(Sc() + 0xC) == 0x2E) Inc(Sc()[2]);
}

// original 0x4DC570: +4 down by two; at 0 +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Dim(void) {
    AddB(Sc()[4], 0xFE);
    if (Sc()[4] == 0) Inc(Sc()[2]);
}

// original 0x4DC590: +0xB down unless 0; +9 down; at 0 the owner's +0xB 0xFF
// (Leech_WaitOrbs's signal) and the task freed.
S27_EXPORT void __cdecl DragonBreathBeam_End(void) {
    if (Sc()[0xB] != 0) Dec(Sc()[0xB]);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The Dragon beam's radii of one quad pair: DamageScratch +0 at the near end
// and +2 at the far, `base` (+0xB x 4, or +0xB x 4 + 0x10 for the outer pair)
// or, over the first steps, `per` x step (near: step - 1 while it is below 4;
// far: step while it is below 4); each plus its end's jitter.
void DragonRadii(unsigned base_add, unsigned per_add, int k, unsigned prev, unsigned cur) {
    const unsigned b = Sc()[0xB];
    const std::uint32_t base = b * 4 + base_add;
    SetSW(2, base);
    SetSW(0, base);
    std::uint32_t far = base;
    if (k - 1 < 4) SetSW(0, (b + per_add) * static_cast<std::uint32_t>(k - 1));
    if (k < 4) far = (b + per_add) * static_cast<std::uint32_t>(k);
    AddSW(0, prev);
    SetSW(2, far + cur);
}
// The four points of a Dragon quad: the side points at angle DamageScratch +4
// and radii +0 / +2 about the step's near (+0xC, +0xE) and far (+8, +0xA)
// centres, at `near_at` and `far_at`; the centres themselves at `near_c` and
// `far_c`.
void DragonSides(unsigned char* p, unsigned near_at, unsigned far_at) {
    PutFloat(p + near_at, SinKAdd(SS(4), 0, 0xC));
    PutFloat(p + near_at + 4, CosKAdd(SS(4), 0, 0xE));
    PutFloat(p + far_at, SinKAdd(SS(4), 2, 8));
    PutFloat(p + far_at + 4, CosKAdd(SS(4), 2, 0xA));
}
void DragonCentres(unsigned char* p, unsigned near_c, unsigned far_c) {
    PutFloat(p + near_c, SS(0xC));
    PutFloat(p + near_c + 4, SS(0xE));
    PutFloat(p + far_c, SS(8));
    PutFloat(p + far_c + 4, SS(0xA));
}
// A point's shade: (s, s, s) on the centre line, (s, s / 4, s / 4) on the
// edge, 1 on the outer edge; s the DamageScratch +6 word, / 4 signed.
void ShadeFull(unsigned char* p, unsigned at) {
    p[at] = SB(6);
    p[at + 1] = SB(6);
    p[at + 2] = SB(6);
}
void ShadeRed(unsigned char* p, unsigned at) {
    p[at] = SB(6);
    p[at + 1] = static_cast<unsigned char>(SS(6) / 4);
    p[at + 2] = static_cast<unsigned char>(SS(6) / 4);
}
void ShadeOne(unsigned char* p, unsigned at) {
    p[at] = 1;
    p[at + 1] = 1;
    p[at + 2] = 1;
}

}  // namespace

// original 0x4DC5D0: the beam - +0xA steps of 12 units from the task's screen
// point along +0x14; each step four semi-transparent POLY_G4, two either side:
// the inner pair from the centre line out to +0xB x 4 (tapered over the first
// four steps), the outer pair from there to +0xB x 4 + 0x10; each edge moved
// by a jitter Rand & 3 (a step's far jitter is the next step's near one);
// shaded by +9 x 15. Tpage 0x35 for the beam, 0x15 after it, slot 3. Screen
// space: the points are written as floats.
S27_EXPORT void __cdecl DragonBreathBeam_Draw(void) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, Word(s + 0x2E));
        SetSW(0xA, Word(s + 0x30));
    }
    unsigned cur = 0;
    for (int k = 1; k < Sc()[0xA] + 1; ++k) {
        SetSW(0xC, SW(8));
        SetSW(0xE, SW(0xA));
        AddSW(8, static_cast<std::uint32_t>(Mul12(Sin(Long(Sc() + 0x14)), 12)));
        AddSW(0xA, static_cast<std::uint32_t>(Mul12(Cos(Long(Sc() + 0x14)), 12)));
        {
            const unsigned char* const s = Sc();
            const unsigned b = s[0xB];
            SetSW(2, b * 4);
            SetSW(0, b * 4);
            if (k - 1 < 4) SetSW(0, static_cast<std::uint32_t>(k - 1) * b);
            if (k < 4) SetSW(2, b * static_cast<std::uint32_t>(k));
            SetSW(6, s[9] * 15u);
        }
        const unsigned prev = cur;
        const int r = RandCall();
        AddSW(0, prev);
        cur = static_cast<unsigned>(r) & 3;
        AddSW(2, cur);
        // the inner pair: the centre line and the edge at +0x400
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) + 0x400u) & 0xFFF);
        DragonSides(p, 0x18, 0x38);
        DragonCentres(p, 8, 0x28);
        ShadeFull(p, 4);
        ShadeFull(p, 0x24);
        ShadeRed(p, 0x14);
        ShadeRed(p, 0x34);
        Commit(3, 0x44);
        // and at -0x400
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) - 0x400u) & 0xFFF);
        DragonSides(p, 8, 0x28);
        DragonCentres(p, 0x18, 0x38);
        ShadeFull(p, 0x14);
        ShadeFull(p, 0x34);
        ShadeRed(p, 4);
        ShadeRed(p, 0x24);
        Commit(3, 0x44);
        // the outer pair at +0x400: from the inner edge out
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) + 0x400u) & 0xFFF);
        DragonRadii(0x10, 4, k, prev, cur);
        DragonSides(p, 0x18, 0x38);
        DragonRadii(0, 0, k, prev, cur);
        DragonSides(p, 8, 0x28);
        ShadeRed(p, 4);
        ShadeRed(p, 0x24);
        ShadeOne(p, 0x14);
        ShadeOne(p, 0x34);
        Commit(3, 0x44);
        // and at -0x400
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) - 0x400u) & 0xFFF);
        DragonRadii(0x10, 4, k, prev, cur);
        DragonSides(p, 8, 0x28);
        DragonRadii(0, 0, k, prev, cur);
        DragonSides(p, 0x18, 0x38);
        ShadeRed(p, 0x14);
        ShadeRed(p, 0x34);
        ShadeOne(p, 4);
        ShadeOne(p, 0x24);
        Commit(3, 0x44);
    }
    ModeCommit(3, 0x15);
}

namespace {

// DragonBreathBeam_DrawLinesA / B: a crackling line of 47 segments (POLY_F2
// lines, semi-transparent) beside the beam, `side` 0x400 or -0x400 from its
// angle. It starts `n` + 1 steps of 12 down the beam (DamageScratch +8 / +0xA,
// kept in the vertex scratch +8 / +0xA), 32 units out; each segment walks
// Rand & 7 along the beam (back to the kept point plus 12 once the walk
// passes 12) and swings out by sin(segment x 0x40) x (0x10 + Rand & 3) >> 12;
// shade ((Rand & 7) + 5) x +4, blue a third of it. Tpage 0x35, 0x15 after,
// slot 3.
void DrawLines(unsigned n, unsigned side) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, Word(s + 0x2E));
        SetSW(0xA, Word(s + 0x30));
    }
    for (unsigned i = (n & 0xFF) + 1; i != 0; --i) {
        AddSW(8, static_cast<std::uint32_t>(Mul12(Sin(Long(Sc() + 0x14)), 12)));
        AddSW(0xA, static_cast<std::uint32_t>(Mul12(Cos(Long(Sc() + 0x14)), 12)));
    }
    SetVW(8, SW(8));
    SetVW(0xA, SW(0xA));
    SetSW(4, (Word(Sc() + 0x14) + side) & 0xFFF);
    SetSW(0xC, SinShlW(SS(4), 5, 8));
    SetSW(0xE, CosShlW(SS(4), 5, 0xA));
    SetSW(0, static_cast<std::uint32_t>(Shl12(Sin(0), 4)));
    SetVW(0, static_cast<std::uint32_t>(SinKAdd(SS(4), 0, 0xC)));
    SetVW(2, static_cast<std::uint32_t>(CosKAdd(SS(4), 0, 0xE)));
    {
        const int r = RandCall();
        SetSW(6, ((static_cast<std::uint32_t>(r) & 7) + 5) * Sc()[4]);
    }
    int walked = 0;
    for (unsigned k = 1; k < 0x30; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint32_t step = static_cast<std::uint32_t>(RandCall()) & 7;
        walked += static_cast<int>(static_cast<short>(step));
        SetSW(0, step);
        if (walked < 12) {
            AddSW(8, static_cast<std::uint32_t>(SinK(Long(Sc() + 0x14), 0)));
            AddSW(0xA, static_cast<std::uint32_t>(CosK(Long(Sc() + 0x14), 0)));
            SetSW(0xC, SinShlW(SS(4), 5, 8));
        } else {
            walked = 0;
            {
                const int v = Sin(Long(Sc() + 0x14));
                SetSW(8, static_cast<std::uint32_t>(Mul12(v, 12)) + VW(8));
            }
            {
                const int c = Cos(Long(Sc() + 0x14));
                const std::uint16_t x = SW(8);
                const std::uint32_t y = static_cast<std::uint32_t>(Mul12(c, 12)) + VW(0xA);
                SetVW(8, x);
                SetSW(0xA, y);
                SetVW(0xA, y);
            }
            SetSW(0xC, SinShlW(SS(4), 5, 8));
        }
        SetSW(0xE, CosShlW(SS(4), 5, 0xA));
        {
            const std::uint32_t a = (k & 0x1F) << 6;
            SetSW(2, a);
            const int v = Sin(static_cast<int>(static_cast<short>(a)));
            const std::uint32_t swing = (static_cast<std::uint32_t>(RandCall()) & 3) + 0x10;
            const int out = static_cast<int>(static_cast<std::uint32_t>(v) * swing) >> 12;
            PutFloat(p + 8, VS(0));
            SetSW(0, static_cast<std::uint32_t>(out));
            PutFloat(p + 0xC, VS(2));
        }
        {
            const auto x = static_cast<short>(SinKAdd(SS(4), 0, 0xC));
            SetVW(0, static_cast<std::uint16_t>(x));
            PutFloat(p + 0x14, x);
        }
        {
            const auto y = static_cast<short>(CosKAdd(SS(4), 0, 0xE));
            SetVW(2, static_cast<std::uint16_t>(y));
            PutFloat(p + 0x18, y);
        }
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = static_cast<unsigned char>(SS(6) / 3);
        Commit(3, 0x20);
    }
    ModeCommit(3, 0x15);
}

}  // namespace

// original 0x4DD0B0: the crackling line at +0x400 (above), its length the
// argument's low byte.
S27_EXPORT void __cdecl DragonBreathBeam_DrawLinesA(unsigned n) { DrawLines(n, 0x400); }

// original 0x4DD4B0: the same at -0x400.
S27_EXPORT void __cdecl DragonBreathBeam_DrawLinesB(unsigned n) { DrawLines(n, 0xFFFFFC00u); }

// ===========================================================================

void MagicS27_Inject() {
    if (bof3::WantsShadow("magic_s27")) magic_s27::SelfTest();
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
    BOF3_INJECT(WhelpBreath_Task);
    BOF3_INJECT(WhelpBreath_Start);
    BOF3_INJECT(WhelpBreath_WaitBeam);
    BOF3_INJECT(WhelpBreathChild_Task);
    BOF3_INJECT(WhelpBreathBeam_Run);
    BOF3_INJECT(WhelpBreathBeam_Aim);
    BOF3_INJECT(WhelpBreathBeam_Grow);
    BOF3_INJECT(WhelpBreathBeam_Draw);
    BOF3_INJECT(WhelpBreathSprite_Run);
    BOF3_INJECT(WhelpBreathSprite_Start);
    BOF3_INJECT(WhelpBreathSprite_Play);
    BOF3_INJECT(WhelpBreathSprite_Hold);
    BOF3_INJECT(WhelpBreathFlames_Grow);
    BOF3_INJECT(WhelpBreathFlames_End);
    BOF3_INJECT(WhelpBreathGlow_Grow);
    BOF3_INJECT(WhelpBreathGlow_Hold);
    BOF3_INJECT(WhelpBreathGlow_End);
    BOF3_INJECT(WhelpBreathFlames_Draw);
    BOF3_INJECT(WhelpBreathFlames_DrawColumn);
    BOF3_INJECT(WhelpBreathFlames_PushMatrix);
    BOF3_INJECT(WhelpBreathGlow_Draw);
    BOF3_INJECT(WhelpBreathGlow_DrawFan);
    BOF3_INJECT(DragonBreath_Task);
    BOF3_INJECT(DragonBreath_Start);
    BOF3_INJECT(DragonBreathBeam_Task);
    BOF3_INJECT(DragonBreathBeam_Run);
    BOF3_INJECT(DragonBreathBeam_Aim);
    BOF3_INJECT(DragonBreathBeam_Widen);
    BOF3_INJECT(DragonBreathBeam_Hit);
    BOF3_INJECT(DragonBreathBeam_Brighten);
    BOF3_INJECT(DragonBreathBeam_Hold);
    BOF3_INJECT(DragonBreathBeam_Dim);
    BOF3_INJECT(DragonBreathBeam_End);
    BOF3_INJECT(DragonBreathBeam_Draw);
    BOF3_INJECT(DragonBreathBeam_DrawLinesA);
    BOF3_INJECT(DragonBreathBeam_DrawLinesB);
}

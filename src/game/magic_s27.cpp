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
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap its +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
void AddSW(unsigned k, std::uint32_t v) { SetSW(k, SW(k) + v); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
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

// ===========================================================================
// MAGIC118 (row 60, Burn read one id down)

// ===========================================================================
// MAGIC120 (row 16, Whelp Breath) and MAGIC121 (row 122, DragonBreath)

namespace {

// The beams' aim, both spells (WhelpBreathBeam_Aim, DragonBreathBeam_Aim once
// +9 runs out): the owner's direction byte; the task to the targets' centre
// (MagicFx_CenterOnSide) and its screen point kept in SharedScratch +0xC /
// +0xE; MagicFx_FormationOffset; the task back at its owner, its screen point
// moved by the words +0xC / +0x10 into +8 / +0xA; +0x14 the angle
// Math_Ratan2(kept x - x, kept y - y) from there to the centre.
void Aim() {
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    MH_CALL(MagicFx_FormationOffset)();
    AtOwner();
    MH_CALL(BattleActor_UpdateScreenXY)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
    const unsigned char* const s = Sc();
    const short x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(8, static_cast<std::uint16_t>(x));
    SetSW(0xA, static_cast<std::uint16_t>(y));
    // Math_Ratan2(y, x) as the original pushes it: first the kept x less x,
    // then the kept y less y.
    const float first = static_cast<float>(SS(0xC) - x);
    const float second = static_cast<float>(SS(0xE) - y);
    const int angle = MH_CALL(Math_Ratan2)(first, second);
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
}

}  // namespace

// ---------------------------------------------------------------------------
// MAGIC121 (row 122, DragonBreath)

// ===========================================================================

void MagicS27_Inject() {
    if (bof3::WantsShadow("magic_s27")) magic_s27::SelfTest();
}

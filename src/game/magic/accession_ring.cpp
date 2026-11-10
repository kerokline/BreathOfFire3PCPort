// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession_ring.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

// move_script::Long is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(move_script::Long(at)) + v)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S33_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0xC a flat quad's, 0x10 a gouraud one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S33_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A flat ring of 64 semi-transparent gouraud quads (tpage 0x35, layer 4)
// between the radii SS(2) = +0x14 + inner_add and SS(4) = +0x14 + outer_add;
// the shade +9 x 15 on the outer edge (vertices 0 and 1) when `bright_outer`,
// else on the inner (vertices 2 and 3), 1 on the other.
void DrawRing(unsigned inner_add, unsigned outer_add, bool bright_outer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(2, Word(s + 0x14) + inner_add);
        SetSW(4, Word(s + 0x14) + outer_add);
        SetSW(0xA, static_cast<unsigned>(s[9]) * 15u);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(a);
        {
            const int w = Mul12(v, SS(4));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(w));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        if (bright_outer) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = SB(0xA);
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(0xA);
        }
        MH_CALL(Gfx_CommitPrim)(4, 0x44);
    }
}

}  // namespace

// original 0x4ECB80: the rings. A call through AccessionRing_Steps (three
// entries: _Start, _Widen, group S29's DivineBeam_Widen) by +2; then while +0
// and +2 are set, under the actor matrix, the inner and the outer ring.
S33_EXPORT void __cdecl AccessionRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AccessionRing_Start, bof3::addr::AccessionRing_Widen,
                                                bof3::addr::DivineBeam_Widen};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AccessionRing_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AccessionRing_DrawInner);
    Call0(bof3::addr::AccessionRing_DrawOuter);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4ECBC0: at the owner; +0x14 0x100, +9 0x10, +2 on.
S33_EXPORT void __cdecl AccessionRing_Start(void) {
    AtOwner();
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4ECC20: the radius +0x14 up by 0x80; at 0x400 +2 on.
S33_EXPORT void __cdecl AccessionRing_Widen(void) {
    AddLong(Sc() + 0x14, 0x80);
    if (rv::Current<records::BattleTask>()->step_y == 0x400) Inc(Sc()[2]);
}

// original 0x4ECC50: the inner ring, from the radius +0x14 to +0x14 + 0x40,
// bright at its outer edge.
S33_EXPORT void __cdecl AccessionRing_DrawInner(void) { DrawRing(0, 0x40, true); }

// original 0x4ECE80: the outer ring, from +0x14 + 0x40 to +0x14 + 0x80, bright
// at its inner edge.
S33_EXPORT void __cdecl AccessionRing_DrawOuter(void) { DrawRing(0x40, 0x80, false); }

void AccessionRing_Inject() {
    BOF3_INJECT(AccessionRing_Run);
    BOF3_INJECT(AccessionRing_Start);
    BOF3_INJECT(AccessionRing_Widen);
    BOF3_INJECT(AccessionRing_DrawInner);
    BOF3_INJECT(AccessionRing_DrawOuter);
}

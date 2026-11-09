// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession_bolt.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s33.h"
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

void AddVW(unsigned k, unsigned v) { SetVW(k, VW(k) + v); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

using Fn3 = void (__cdecl*)(int, int, int);

void Call3(std::uint32_t address, int a, int b, int c) { MH_WORD(Fn3, address)(a, b, c); }

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

void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EC500: the bolt. A call through AccessionBolt_Steps (four
// entries: _Start, _Rise, group S22's MyollnirBolt_Hold and JoltBolt_End) by
// +2, the screen point; then while +2 and +0 are set, under group S25's turn
// matrix, three band rows ((0x20, 0x20, 7), (0x10, 0x40, 0xF), (0x10, 0x60,
// 0x1F), the angle stepping by 4 and 4, then back).
S33_EXPORT void __cdecl AccessionBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::AccessionBolt_Start, bof3::addr::AccessionBolt_Rise,
                                                bof3::addr::MyollnirBolt_Hold, bof3::addr::JoltBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("AccessionBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        const unsigned char* const s = Sc();
        if (s[2] == 0 || s[0] == 0) return;
    }
    MH_CALL(SpellSleep_PushTurnMatrix)();
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x20, 0x20, 7);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x10, 0x40, 0xF);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x10, 0x60, 0x1F);
    AddB(Sc()[0xB], 0xF8);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EC580: +9 down; at 0: at the owner, +0xB Rand & 0xF, +9 and +0xA
// 0, +2 on.
S33_EXPORT void __cdecl AccessionBolt_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    AtOwner();
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4EC600: +0xB up, +0xA up by 4; at 0x10 +9 8 and +2 on.
S33_EXPORT void __cdecl AccessionBolt_Rise(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] != 0x10) return;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4EC640: one row of the bolt - 17 steps of four semi-transparent
// gouraud quads (tpage 0x35): the radius s16 a2 plus or minus Rand & a3 (the
// first s16 a2 / 2), the corners a1 in, each step 0x40 higher, the angle
// ((+0xB + i) & 0xF) << 8; the shades +0xA x 15 and +0xA x 13 (Scratch_Swap),
// read back at every use; the first step's far edges dark.
S33_EXPORT void __cdecl AccessionBolt_DrawBand(int a1, int a2, int a3) {
    const auto in = static_cast<std::uint16_t>(a1);
    const auto in2 = static_cast<std::uint16_t>(static_cast<std::uint32_t>(a1) * 2);
    {
        const int r = static_cast<short>(a2);
        SetSW(0, static_cast<unsigned>(r / 2));
    }
    {
        const unsigned char* const s = Sc();
        SetSW(0xE, (s[0xB] & 0xFu) << 8);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) * 15u);
        SetSW(0xC, static_cast<unsigned>(s[0xA]) * 13u);
    }
    int v = MH_CALL(Math_Sin)(SS(0xE));
    {
        unsigned char* const p = Gfx_PacketNext;
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(4, 0);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
    }
    LinkAtSprite(0xC);
    for (int i = 1; i < 0x12; ++i) {
        SetSW(0xE, ((Sc()[0xB] + static_cast<unsigned>(i)) & 0xFu) << 8);
        std::uint32_t r = RandCall();
        if (r & 1) {
            r = RandCall();
            SetSW(0, (r & static_cast<std::uint32_t>(a3)) + static_cast<std::uint32_t>(a2));
        } else {
            r = RandCall();
            SetSW(0, static_cast<std::uint32_t>(a2) - (r & static_cast<std::uint32_t>(a3)));
        }
        {
            const std::uint16_t top = VW(2), h = VW(4);
            SetVW(0x12, top);
            SetVW(0x1A, static_cast<unsigned>(top) - in);
            SetVW(0x10, 0);
            SetVW(0x14, h);
            SetVW(0x18, 0);
            SetVW(0x1C, h);
            SetVW(0, 0);
        }
        v = MH_CALL(Math_Sin)(SS(0xE));
        SetVW(8, 0);
        unsigned char* p = Gfx_PacketNext;
        {
            const int y = Mul12(v, SS(0));
            const unsigned up = (0u - static_cast<unsigned>(i)) << 6;
            SetVW(2, static_cast<unsigned>(y));
            SetVW(4, up);
            SetVW(0xA, static_cast<unsigned>(y) - in);
            SetVW(0xC, up);
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        p[0x14] = SB(0xC);
        p[0x15] = 1;
        p[0x16] = SB(0xC);
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x24] = SB(0xA);
            p[0x25] = SB(0xA);
            p[0x26] = SB(0xA);
            p[0x34] = SB(0xC);
            p[0x35] = 1;
            p[0x36] = SB(0xC);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, 0u - in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[5] = 1;
        p[4] = SB(0xC);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u}) p[k] = 1;
        } else {
            p[0x25] = 1;
            p[0x24] = SB(0xC);
            p[0x26] = SB(0xC);
        }
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in2);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[5] = 1;
        p[4] = SB(0xC);
        p[6] = SB(0xC);
        p[0x14] = SB(0xA);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xA);
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x25] = 1;
            p[0x24] = SB(0xC);
            p[0x26] = SB(0xC);
            p[0x34] = SB(0xA);
            p[0x35] = SB(0xA);
            p[0x36] = SB(0xA);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(0xC);
        p[0x15] = 1;
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        if (i == 1) {
            for (unsigned k : {0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x35] = 1;
            p[0x34] = SB(0xC);
            p[0x36] = SB(0xC);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        AddVW(2, 0u - in2);
    }
}

void AccessionBolt_Inject() {
    BOF3_INJECT(AccessionBolt_Run);
    BOF3_INJECT(AccessionBolt_Start);
    BOF3_INJECT(AccessionBolt_Rise);
    BOF3_INJECT(AccessionBolt_DrawBand);
}

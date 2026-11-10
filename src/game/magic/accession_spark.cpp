// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession_spark.h"

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

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(move_script::Long(at)) + v)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

using Fn2 = void (__cdecl*)(unsigned, unsigned);

void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_WORD(Fn2, address)(a, b); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers gets the
// flag pointer the originals pass; the four-point projection the depth and
// flag pointers.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

#define S33_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    S33_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EC0F0: a spark. A call through AccessionSpark_Steps (three
// entries) by +2; then while +0 and +2 are set, under the actor matrix: +0xC
// up, +9 up by 2, the lines at the angle pair 0x65BEA0[+4] (the first plus
// +9), unchecked.
S33_EXPORT void __cdecl AccessionSpark_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AccessionSpark_Start, bof3::addr::AccessionSpark_Grow,
                                                bof3::addr::AccessionSpark_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AccessionSpark_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    AddLong(Sc() + 0xC, 1);
    AddB(Sc()[9], 2);
    {
        const unsigned char* const s = Sc();
        const unsigned char* const pair = Mem(::bof3::Img(AccessionSpark_Angles_addr) + static_cast<unsigned>(s[4]) * 2u);
        Call2(bof3::addr::AccessionSpark_Draw, static_cast<unsigned char>(pair[0] + s[9]), pair[1]);
    }
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EC160: at the owner; +0xC Rand & 0x1F, +0x5D 0x10, +0xB and +9
// 0, +0xA 4, +2 on.
S33_EXPORT void __cdecl AccessionSpark_Start(void) {
    AtOwner();
    const std::uint32_t r = RandCall();
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(r & 0x1F));
    Sc()[0x5D] = 0x10;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4EC1E0: the line count +0xA up by 4; from 0x20 +0xB 1 and +2 on.
S33_EXPORT void __cdecl AccessionSpark_Grow(void) {
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] < 0x20) return;
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4EC210: the shade +0x5D down by 2; at 0 the owner's count +0xB
// down and the task freed.
S33_EXPORT void __cdecl AccessionSpark_Fade(void) {
    AddB(Sc()[0x5D], 0xFE);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EC240: the spark's +0xA lines (the count read back each line),
// each a semi-transparent flat line (tpage (+0xB & 3) << 5 | 0x15, mode +0xB)
// from the previous end to a point at radius 0x200 whose latitude wobbles
// about a2 (Rand & 3 + 4 by a sine of ((+0xC + i) & 0x1F) << 7) and whose
// longitude steps by Rand & 7 from a1 (the argument slot keeps the sum);
// shaded +0x5D x 12, linked at its end's world position.
S33_EXPORT void __cdecl AccessionSpark_Draw(unsigned a1, unsigned a2) {
    auto along = static_cast<unsigned char>(a1);
    const unsigned lat = a2 & 0xFF;
    SetSW(0xA, static_cast<unsigned>(SignedByte(Sc()[0x5D]) * 12));
    SetSW(0, 0x200);
    int v = MH_CALL(Math_Sin)(static_cast<int>(lat << 4));
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(static_cast<int>(static_cast<unsigned>(along) << 4));
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(static_cast<int>(static_cast<unsigned>(along) << 4));
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(static_cast<int>(lat << 4));
    SetVW(0xC, static_cast<unsigned>(Mul12(v, SS(0))));
    if (Sc()[0xA] == 0) return;
    for (unsigned i = 0;;) {
        {
            const std::uint16_t h = VW(0xC), x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
            SetVW(4, h);
        }
        std::uint32_t r = RandCall();
        SetSW(4, (r & 3) + 4);
        {
            const auto turn = static_cast<unsigned short>(((Sc()[0xC] + i) & 0x1F) << 7);
            SetSW(0xE, turn);
            v = MH_CALL(Math_Sin)(static_cast<short>(turn));
        }
        {
            const auto wobble = static_cast<short>(Shl(Mul12(v, SS(4)) + static_cast<int>(lat), 4));
            SetSW(0xE, static_cast<unsigned short>(wobble));
            v = MH_CALL(Math_Sin)(wobble);
        }
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        SetVW(0xC, static_cast<unsigned>(Mul12(v, SS(0))));
        r = RandCall();
        along = static_cast<unsigned char>(along + (r & 7));
        const int lon = static_cast<int>(static_cast<unsigned>(along) << 4);
        v = MH_CALL(Math_Sin)(lon);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(lon);
        const auto z = static_cast<short>(Mul12(v, SS(2)));
        SetVW(0xA, static_cast<unsigned short>(z));
        const short vx = VS(8);
        std::uint32_t wx, wz;
        unsigned tpage;
        {
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            wx = Shl(vx, 9) + U(ts->x);
            tpage = ((s[0xB] & 3u) << 5) | 0x15u;
            wz = Shl(z, 9) + U(ts->z);
        }
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, Sc()[0xB]);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        Rtp1(8, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 2, 0x20);
        ++i;
        if (static_cast<unsigned char>(i) >= Sc()[0xA]) break;
    }
}

void AccessionSpark_Inject() {
    BOF3_INJECT(AccessionSpark_Run);
    BOF3_INJECT(AccessionSpark_Start);
    BOF3_INJECT(AccessionSpark_Grow);
    BOF3_INJECT(AccessionSpark_Fade);
    BOF3_INJECT(AccessionSpark_Draw);
}

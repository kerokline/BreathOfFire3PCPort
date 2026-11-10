// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/magic/combustion_flash.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F8030: a call through CombustionFlash_Steps (five: _Start,
// group S30's MagicFx_CountUp9By2, _Hold, _Spin, _End) by +2; then while +0
// and +2 are not 0 the flash drawn twice (a call, then a tail jmp).
S37_EXPORT void __cdecl CombustionFlash_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::CombustionFlash_Start, addr::MagicFx_CountUp9By2,
                                                addr::CombustionFlash_Hold, addr::CombustionFlash_Spin,
                                                addr::CombustionFlash_End};
    Dispatch(kSteps, 5, Sc()[2], "CombustionFlash_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::CombustionFlash_Draw);
    Call0(addr::CombustionFlash_Draw);
}

// original 0x4F8060: the flash at its owner's screen point (words +0x2E /
// +0x30); the target flagged 0x10; +0xB 8, +9 0, +0xA 4, +2 on.
S37_EXPORT void __cdecl CombustionFlash_Start(void) {
    {
        unsigned char* const s = Sc();
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_y, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    }
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    unsigned char* const s = Sc();
    s[0xB] = 8;
    s[9] = 0;
    s[0xA] = 4;
    Inc(s[2]);
}

// original 0x4F80C0: +0xA down, at 0 +0xA 0x10 and +2 on.
S37_EXPORT void __cdecl CombustionFlash_Hold(void) {
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    s[0xA] = 0x10;
    Inc(s[2]);
}

// original 0x4F80F0: the radius +0xB up by 4; +0xA down, at 0 +2 on.
S37_EXPORT void __cdecl CombustionFlash_Spin(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 4);
    Dec(s[0xA]);
    if (s[0xA] == 0) Inc(s[2]);
}

// original 0x4F8120: the radius +0xB up by 4; +9 down, at 0 the owner's count
// +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionFlash_End(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 4);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F8160: a draw-mode packet (tpage 0x35) on layer 2; the words
// 0x903850 the radius +0xB, 0x903852 twice it, 0x90385A +9 x 15, 0x90385C and
// 0x90385E +9 x 8; then 32 steps across the screen from the sprite's screen
// point less 0x100 (the word +0x2E), 0x10 wide, the angle 0x40 a step: four
// semi-transparent gouraud quads a step in screen space (floats), each on
// layer 2 - above the line y (the word +0x30) a band to r sin (grey at the
// line, the three colours at the edge) and a band from r sin to 2r sin (the
// three colours inside, 1 outside), and the same two below. The radius words
// are read again after every Math_Sin, as the original reads them.
S37_EXPORT void __cdecl CombustionFlash_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    std::uint32_t step_x;
    int y;
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, s[9] * 15u);
        SetSW(0xC, static_cast<unsigned>(s[9]) << 3);
        SetSW(0xE, static_cast<unsigned>(s[9]) << 3);
        step_x = (static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) - 0x100u) & 0xFFFF;
        y = S16(s + 0x30);
    }
    unsigned char fy[4];
    PutFloat(fy, y);
    // The three colour bytes, read at each store.
    auto top = [](unsigned char* p, unsigned at) {
        p[at] = SB(0xA);
        p[at + 1] = SB(0xC);
        p[at + 2] = SB(0xE);
    };
    auto grey = [](unsigned char* p, unsigned at) {
        p[at] = SB(0xA);
        p[at + 1] = SB(0xA);
        p[at + 2] = SB(0xA);
    };
    auto dark = [](unsigned char* p, unsigned at) {
        p[at] = 1;
        p[at + 1] = 1;
        p[at + 2] = 1;
    };
    int a = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        const int x0 = static_cast<short>(step_x);
        const int x1 = x0 + 0x10;
        const int a1 = a + 0x40;
        unsigned char f0[4], f1[4];
        PutFloat(f0, x0);
        PutFloat(f1, x1);
        // above the line, the inner band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int ya = y + Mul12(v, SS(0));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, ya);
            std::memcpy(p + 0x2C, fy, 4);
            std::memcpy(p + 0x18, f1, 4);
            v = Sin(a1);
            const int yb = y + Mul12(v, SS(0));
            std::memcpy(p + 0x38, f1, 4);
            std::memcpy(p + 0x3C, fy, 4);
            PutFloat(p + 0x1C, yb);
            top(p, 4);
            top(p, 0x14);
            grey(p, 0x24);
            grey(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // above, the outer band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int y0 = y + Mul12(v, SS(2));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, y0);
            v = Sin(a);
            const int y2 = y + Mul12(v, SS(0));
            std::memcpy(p + 0x18, f1, 4);
            PutFloat(p + 0x2C, y2);
            v = Sin(a1);
            const int y1 = y + Mul12(v, SS(2));
            std::memcpy(p + 0x38, f1, 4);
            PutFloat(p + 0x1C, y1);
            v = Sin(a1);
            const int y3 = y + Mul12(v, SS(0));
            dark(p, 4);
            dark(p, 0x14);
            PutFloat(p + 0x3C, y3);
            top(p, 0x24);
            top(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // below the line, the inner band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int ya = y - Mul12(v, SS(0));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, ya);
            std::memcpy(p + 0x2C, fy, 4);
            std::memcpy(p + 0x18, f1, 4);
            v = Sin(a1);
            const int yb = y - Mul12(v, SS(0));
            std::memcpy(p + 0x38, f1, 4);
            std::memcpy(p + 0x3C, fy, 4);
            PutFloat(p + 0x1C, yb);
            top(p, 4);
            top(p, 0x14);
            grey(p, 0x24);
            grey(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // below, the outer band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int y0 = y - Mul12(v, SS(2));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, y0);
            v = Sin(a);
            const int y2 = y - Mul12(v, SS(0));
            std::memcpy(p + 0x18, f1, 4);
            PutFloat(p + 0x2C, y2);
            v = Sin(a1);
            const int y1 = y - Mul12(v, SS(2));
            std::memcpy(p + 0x38, f1, 4);
            PutFloat(p + 0x1C, y1);
            v = Sin(a1);
            const int y3 = y - Mul12(v, SS(0));
            dark(p, 4);
            dark(p, 0x14);
            PutFloat(p + 0x3C, y3);
            top(p, 0x24);
            top(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        step_x += 0x10;
        a = a1;
    }
}

void CombustionFlash_Inject() {
    BOF3_INJECT(CombustionFlash_Run);
    BOF3_INJECT(CombustionFlash_Start);
    BOF3_INJECT(CombustionFlash_Hold);
    BOF3_INJECT(CombustionFlash_Spin);
    BOF3_INJECT(CombustionFlash_End);
    BOF3_INJECT(CombustionFlash_Draw);
}

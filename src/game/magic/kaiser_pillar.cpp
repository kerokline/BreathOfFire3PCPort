// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/kaiser_pillar.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s30.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
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

// The scratch the overlays keep their working values in: sixteen bytes at
// 0x903850 (words; Scratch_Swap is +0xC) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
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

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return Low32(p); }

// `imul` then `sar 0xC` (or `sar 3`): the 32-bit product wraps, the shift is
// arithmetic.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}

int Mul12(int a, int b) { return MulSar(a, b, 12); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S30_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp4(unsigned char* prim) {
    long depth, flag;
    S30_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &depth, &flag);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A .data dispatch table read in place (the fuzz swaps its entries for
// recorders), the index checked against the table's length.
magic_harness::Handler Entry(const char* who, std::uint32_t table, unsigned entries, unsigned index) {
    if (index >= entries) PastTable(who, index, entries);
    return reinterpret_cast<magic_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E5970: once the owner's +9 is 0 and File_LoadDone, +0xA 8 and
// +2 on.
S30_EXPORT void __cdecl KaiserFlash_WaitLoad(void) {
    if (Owner()[9] != 0) return;
    if (MH_CALL(File_LoadDone)() == 0) return;
    Sc()[0xA] = 8;
    Inc(Sc()[2]);
}

// original 0x4E59A0: a semi-transparent flat quad over the screen (0..319 x
// 0..239), shade +9 x 15, between draw modes of tpage 0x35 and 0x15.
S30_EXPORT void __cdecl KaiserFlash_Draw(void) {
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyF4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    constexpr std::int32_t kBottom = 0x436F0000;   // 239.0f
    // DIV-0041: x from Widescreen_FillX() to 319 + the columns (the PSP's
    // -32..351); 0..319 while the fills are unarmed, as the fuzz compares.
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    SetLong(p + 8, left);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, right);
    SetLong(p + 0x18, 0);
    SetLong(p + 0x20, left);
    SetLong(p + 0x24, kBottom);
    SetLong(p + 0x2C, right);
    SetLong(p + 0x30, kBottom);
    const auto shade = static_cast<unsigned char>(Sc()[9] * 15);
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    Commit(2, 0x38);
    DrawMode(Gfx_PacketNext, 0x15);
    Commit(2, 0xC);
}

// original 0x4E5A40: +2 through KaiserPillar_Phases (three entries); while +0
// and +2 are set, the pillar under the actor's matrix.
S30_EXPORT void __cdecl KaiserPillar_Run(void) {
    Entry("KaiserPillar_Run", Addr(KaiserPillar_Phases), 3, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::KaiserPillar_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E5A80: at (Field_Kind2X, Field_Kind2Z), the owner's height;
// shade +0x5D 0x10, +9 0x20, +0xA 8, +2 on.
S30_EXPORT void __cdecl KaiserPillar_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, Field_Kind2X);
    rv::Store(rv::Current<records::BattleTask>()->z, Field_Kind2Z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0x5D] = 0x10;
    Sc()[9] = 0x20;
    Sc()[0xA] = 8;
    Inc(Sc()[2]);
}

// original 0x4E5AE0: +9 up by 4; on at 0x40.
S30_EXPORT void __cdecl KaiserPillar_Grow(void) {
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x40) Inc(Sc()[2]);
}

// original 0x4E5B00: +9 up by 2, the shade +0x5D down by 4; at 0 the owner's
// count down and the task freed.
S30_EXPORT void __cdecl KaiserPillar_Fade(void) {
    AddB(Sc()[9], 2);
    AddB(Sc()[0x5D], 0xFC);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5B40: eight rings of sixteen textured quads (fourteen drawn:
// the 10th and 11th of each ring are skipped), each ring from radius +9 x 16
// - 0x40 j out to 0x40 more, its heights -(sin(0x100 j) x +0xA x 16 >> 12)
// of the ring before and the new; u by the quad's parity and j / 4, v and
// its height from KaiserPillar_V / _H[j]; shade the signed +0x5D x 8.
S30_EXPORT void __cdecl KaiserPillar_Draw(void) {
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(3, 0xC);
    {
        const unsigned char* const sc = Sc();
        const unsigned shade = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(sc[0x5D]))) << 3;
        SetSW(8, shade);
        SetSW(0xA, shade);
        SetSW(0xC, shade);
        SetSW(0, sc[0xA] << 4);
        SetSW(2, sc[9] << 4);
    }
    int v = Sin(0);
    SetVW(0xC, static_cast<unsigned>(-Mul12(v, SS(0))));
    int outer = 0x100;
    for (unsigned j = 0; j < 8; ++j, outer += 0x100) {
        const std::uint16_t r = SW(2);
        SetSW(4, r);
        SetSW(2, r - 0x40);
        v = Sin(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Cos(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        const std::uint16_t top = VW(0xC);
        SetVW(0x14, top);
        SetVW(0x1C, top);
        v = Sin(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Sin(outer);
        const auto height = static_cast<unsigned>(-Mul12(v, SS(0)));
        SetVW(4, height);
        SetVW(0xC, height);
        const auto band = static_cast<unsigned char>((j >> 2) << 1);
        const unsigned char tv = Mem(Addr(KaiserPillar_V))[j];
        const unsigned char th = Mem(Addr(KaiserPillar_H))[j];
        for (unsigned q = 1; q < 0x11; ++q) {
            const unsigned angle = (q & 0xF) << 8;
            SetSW(6, angle);
            SetVW(0, VW(8));
            SetVW(2, VW(0xA));
            v = Sin(static_cast<int>(angle));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = Cos(SS(6));
            const std::uint16_t x18 = VW(0x18), x1a = VW(0x1A);
            const int next = SS(6);
            SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
            SetVW(0x10, x18);
            SetVW(0x12, x1a);
            v = Sin(next);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = Cos(SS(6));
            unsigned char* const p = Gfx_PacketNext;
            SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
            MH_CALL(Gpu_SetPolyFT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage & 0xFFFF);
            const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut & 0xFFFF);
            const auto u = static_cast<unsigned char>(((q & 1) + band) << 6);
            p[0x14] = u;
            p[0x15] = tv;
            p[0x24] = static_cast<unsigned char>(u + 0x3F);
            p[0x25] = tv;
            p[0x34] = u;
            p[0x35] = static_cast<unsigned char>(tv + th - 1);
            p[0x44] = static_cast<unsigned char>(u + 0x3F);
            p[0x45] = static_cast<unsigned char>(tv + th - 1);
            Rtp4(p);
            MH_CALL(Gte_PrimDepths4_10)(p);
            p[4] = SB(8);
            p[5] = SB(0xA);
            p[6] = SB(0xC);
            if (q <= 9 || q >= 0xC) Commit(3, 0x48);
        }
    }
}

void KaiserPillar_Inject() {
    BOF3_INJECT(KaiserFlash_WaitLoad);
    BOF3_INJECT(KaiserFlash_Draw);
    BOF3_INJECT(KaiserPillar_Run);
    BOF3_INJECT(KaiserPillar_Start);
    BOF3_INJECT(KaiserPillar_Grow);
    BOF3_INJECT(KaiserPillar_Fade);
    BOF3_INJECT(KaiserPillar_Draw);
}

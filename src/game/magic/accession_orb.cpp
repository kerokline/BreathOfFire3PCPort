// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession_orb.h"

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

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

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

// A new kind-1 child (0x44) owned by this task with phase `phase`, counted in
// +0xB; the child's slot.
unsigned char* Child44(unsigned char phase) {
    const unsigned slot = NewTask(0x44);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = phase;
    return child;
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EB8C0: the orb. A call through AccessionOrb_Steps (six entries)
// by +2; then while +0 and +2 are set, under the actor matrix, its shell and
// its glow.
S33_EXPORT void __cdecl AccessionOrb_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::AccessionOrb_Start, bof3::addr::AccessionOrb_Grow,
                                                bof3::addr::AccessionOrb_Emit, bof3::addr::AccessionOrb_WaitChildren,
                                                bof3::addr::AccessionOrb_Dim, bof3::addr::AccessionOrb_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("AccessionOrb_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AccessionOrb_DrawShell);
    Call0(bof3::addr::AccessionOrb_DrawGlow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EB900: at the owner; +0x5D and +0x5E 0x10, +0x5F 0x24, +4, +0xB
// and +9 0, +0xA 0x1E, +2 on.
S33_EXPORT void __cdecl AccessionOrb_Start(void) {
    AtOwner();
    Sc()[0x5D] = 0x10;
    Sc()[0x5E] = 0x10;
    Sc()[0x5F] = 0x24;
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4EB980: the radius +9 up by 2; at 0x20 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Grow(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x20) Inc(Sc()[2]);
}

// original 0x4EB9A0: every fourth frame a spark child (+1 1, +4 this task's +4
// & 0xF) counted in +0xB, +4 up; while the owner is at its step 5, +0xA down,
// at 0 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Emit(void) {
    if ((static_cast<unsigned char>(Frame_Counter) & 3) == 0) {
        unsigned char* const child = Child44(1);
        unsigned char* const s = Sc();
        child[4] = static_cast<unsigned char>(s[4] & 0xF);
        Inc(s[0xB]);
        Inc(Sc()[4]);
    }
    if (Owner()[2] != 5) return;
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4EBA30: once every spark has ended (+0xB 0), +2 on.
S33_EXPORT void __cdecl AccessionOrb_WaitChildren(void) {
    if (Sc()[0xB] == 0) Inc(Sc()[2]);
}

// original 0x4EBA40: the shell's shade +0x5D down; at 8 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Dim(void) {
    Dec(Sc()[0x5D]);
    if (Sc()[0x5D] == 8) Inc(Sc()[2]);
}

// original 0x4EBA60: the shell's shade down to 0, the glow's +0x5E down; at 0
// the owner's count +0xB down and the task freed.
S33_EXPORT void __cdecl AccessionOrb_Fade(void) {
    if (Sc()[0x5D] != 0) Dec(Sc()[0x5D]);
    Dec(Sc()[0x5E]);
    if (Sc()[0x5E] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EBAA0: the orb's shell - a dome of radius +9 x 16 from latitude
// 0x400 to 0xB80 in eight bands of sixteen semi-transparent flat quads (tpage
// 0x55), each shaded +0x5D x 15 and linked at its corner's world position.
S33_EXPORT void __cdecl AccessionOrb_DrawShell(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(SignedByte(s[0x5D]) * 15));
        SetSW(0, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0x400);
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0x400);
    SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
    int angle = 0x480;
    for (int bands = 8; bands != 0; --bands, angle += 0x80) {
        {
            const std::uint16_t h = SW(6), r = SW(2);
            SetSW(4, r);
            SetSW(8, h);
        }
        v = MH_CALL(Math_Sin)(angle);
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Sin)(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        {
            const std::uint16_t h = SW(6);
            SetVW(4, h);
            SetVW(0xC, h);
        }
        v = MH_CALL(Math_Sin)(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        {
            const std::uint16_t h = SW(8);
            SetVW(0x14, h);
            SetVW(0x1C, h);
        }
        for (unsigned i = 1; i < 0x11; ++i) {
            const unsigned turn = (i & 0xF) << 8;
            {
                const std::uint16_t x = VW(8), y = VW(0xA);
                SetSW(0xE, turn);
                SetVW(0, x);
                SetVW(2, y);
            }
            v = MH_CALL(Math_Sin)(static_cast<int>(turn));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = MH_CALL(Math_Cos)(SS(0xE));
            {
                const int w = Mul12(v, SS(2));
                const short e = SS(0xE);
                const std::uint16_t x = VW(0x18);
                SetVW(0xA, static_cast<unsigned>(w));
                const std::uint16_t y = VW(0x1A);
                SetVW(0x10, x);
                SetVW(0x12, y);
                v = MH_CALL(Math_Sin)(e);
            }
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = MH_CALL(Math_Cos)(SS(0xE));
            const auto z = static_cast<short>(Mul12(v, SS(4)));
            const short vx = VS(0x18);
            SetVW(0x1A, static_cast<unsigned short>(z));
            unsigned char* p = Gfx_PacketNext;
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            const std::uint32_t wx = Shl(vx, 9) + U(ts->x);
            const std::uint32_t wz = Shl(z, 9) + U(ts->z);
            MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x55, 0);
            MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0xC);
            p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Rtp4(p, 0xC);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            p[4] = SB(0xA);
            p[5] = SB(0xA);
            p[6] = SB(0xA);
            MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0x38);
        }
    }
}

// original 0x4EBDB0: the orb's glow - one band of sixteen semi-transparent
// gouraud quads (tpage 0x35) between latitude 0x400 at radius +9 x 16 and a
// latitude (Rand & 3 + +0x5F) x 32 at radius +9 x 18, dark at the inner edge,
// (+0x5E x 12, +0x5E x 8, +0x5E x 12) at the outer.
S33_EXPORT void __cdecl AccessionOrb_DrawGlow(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(SignedByte(s[0x5E]) * 12));
        SetSW(0xC, Shl(SignedByte(s[0x5E]), 3));
        SetSW(0, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0x400);
    SetSW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0x400);
    SetSW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    SetSW(0, static_cast<unsigned>(Sc()[9]) * 18u);
    {
        const std::uint32_t r = RandCall();
        const auto lat = static_cast<short>(Shl(static_cast<int>(r & 3) + SignedByte(Sc()[0x5F]), 5));
        SetSW(0xE, static_cast<unsigned short>(lat));
        v = MH_CALL(Math_Sin)(lat);
    }
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(SS(0xE));
    SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    {
        const std::uint16_t h = SW(6);
        SetVW(4, h);
        SetVW(0xC, h);
    }
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
    {
        const std::uint16_t h = SW(8);
        SetVW(0x14, h);
        SetVW(0x1C, h);
    }
    for (unsigned i = 1; i < 0x11; ++i) {
        const unsigned turn = (i & 0xF) << 8;
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetSW(0xE, turn);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = MH_CALL(Math_Sin)(static_cast<int>(turn));
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        {
            const int w = Mul12(v, SS(2));
            const short e = SS(0xE);
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(w));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
            v = MH_CALL(Math_Sin)(e);
        }
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        const auto z = static_cast<short>(Mul12(v, SS(4)));
        const short vx = VS(0x18);
        SetVW(0x1A, static_cast<unsigned short>(z));
        unsigned char* p = Gfx_PacketNext;
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const std::uint32_t wx = Shl(vx, 9) + U(ts->x);
        const std::uint32_t wz = Shl(z, 9) + U(ts->z);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xA);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xC);
        p[0x36] = SB(0xA);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0x44);
    }
}

void AccessionOrb_Inject() {
    BOF3_INJECT(AccessionOrb_Run);
    BOF3_INJECT(AccessionOrb_Start);
    BOF3_INJECT(AccessionOrb_Grow);
    BOF3_INJECT(AccessionOrb_Emit);
    BOF3_INJECT(AccessionOrb_WaitChildren);
    BOF3_INJECT(AccessionOrb_Dim);
    BOF3_INJECT(AccessionOrb_Fade);
    BOF3_INJECT(AccessionOrb_DrawShell);
    BOF3_INJECT(AccessionOrb_DrawGlow);
}

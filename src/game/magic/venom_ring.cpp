// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/venom_ring.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return Low32(p); }

std::int32_t PtrValue(const void* p) {
    return static_cast<std::int32_t>(Low32(p));
}

// `imul` then `sar 0xC` (or `sar 3`): the 32-bit product wraps, the shift is
// arithmetic.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}

int Mul12(int a, int b) { return MulSar(a, b, 12); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

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

// Gfx_ClutStrip / Gfx_ClutStripSource word indices: row 26 (0x812980 /
// 0x80E980), row 1 (0x80F980 / 0x80B980) and row 1 + 0x21 (0x80F9C2).
constexpr unsigned kRow26 = 0x1A00, kRow1 = 0x200;

// The sprite-control fields both sprite children's starts write, in their
// order (no call between: the order is the originals' for reading only).
void SpriteFields(unsigned char c25, unsigned char c27, unsigned char shade, unsigned char c5c) {
    Sc()[0x25] = c25;
    Sc()[0x26] = 0;
    Sc()[0x27] = c27;
    Sc()[0x28] = 1;
    Sc()[0x24] = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x10000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x10000);
    Sc()[0x48] = 0;
    Sc()[0] = static_cast<unsigned char>(Sc()[0] | 0x20);
    Sc()[0x5C] = c5c;
    Sc()[0x5D] = shade;
    Sc()[0x5E] = shade;
    Sc()[0x5F] = shade;
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E4420: the kind-2 task. A three-entry stack table by +1:
// Venom_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish.
S30_EXPORT void __cdecl Venom_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Venom_Start, bof3::addr::MagicFx_CountDownFlag10,
                                                 bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Venom_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E4450: the task at the side's centre, the owner's direction
// byte, +0xB 0, +9 0x1E, +1 on; one kind-1 child 0x66 of kind 0 (the sprite)
// and six of kind 1 (the rings, +0xB their index), each with +0x80 this task
// and +0xB counting them; CLUT row 26 from its source with the stp bit, but
// its first entry without; sounds 0x100 and 0x101.
S30_EXPORT void __cdecl Venom_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[8] = Owner()[8];
    Sc()[0xB] = 0;
    Sc()[9] = 0x1E;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x66);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = PtrValue(self);
        child[1] = 0;
        Inc(self[0xB]);
    }
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(0x66);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = PtrValue(self);
        child[1] = 1;
        child[0xB] = static_cast<unsigned char>(i);
        Inc(self[0xB]);
    }
    for (unsigned k = 0; k < 0x100; ++k)
        Gfx_ClutStrip[kRow26 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow26 + k] | 0x8000);
    Gfx_ClutStrip[kRow26] = Gfx_ClutStripSource[kRow26];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Sound_PlayById)(0x101);
}

// original 0x4E4540: the children's kind-1 task, a jmp through
// Venom_ChildKinds (two entries: the sprite, the ring) by +1.
S30_EXPORT void __cdecl VenomChild_Task(void) {
    Entry("VenomChild_Task", Addr(Venom_ChildKinds), 2, Sc()[1])();
}

// original 0x4E4560 (shared with MAGIC126 and MAGIC129): the sprite bank
// 0x9039D8 at 0x8E3580 around its phase (+2 through VenomSprite_Phases, four
// entries); Sprite_UpdateScreen while +0 bit 0 and +2 are set.
S30_EXPORT void __cdecl VenomSprite_Run(void) {
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    Entry("VenomSprite_Run", Addr(VenomSprite_Phases), 4, Sc()[2])();
    const unsigned char* const sc = Sc();
    if ((sc[0] & 1) != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4E45A0: the sprite at screen (0xDC, 0x6E), its control fields,
// shade 0x80, animation 0; +9 0x3C, +0xA 0xFF, +2 on.
S30_EXPORT void __cdecl VenomSprite_Start(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, 0xDC);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, 0x6E);
    SpriteFields(0x1D, 0x1A, 0x80, 1);
    Sc()[0x2A] = 0;
    Sc()[0x29] = 3;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0x3C;
    Sc()[0xA] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4E4690: the shade +0x5D..+0x5F up by 8; on once +0x5D is 0.
S30_EXPORT void __cdecl VenomSprite_ShadeUp(void) {
    AddB(Sc()[0x5D], 8);
    AddB(Sc()[0x5E], 8);
    AddB(Sc()[0x5F], 8);
    if (Sc()[0x5D] == 0) Inc(Sc()[2]);
}

// original 0x4E46D0: the shade down by 8; at +0x5D 0x80 the owner's count
// down and the task freed.
S30_EXPORT void __cdecl VenomSprite_ShadeDown(void) {
    AddB(Sc()[0x5D], 0xF8);
    AddB(Sc()[0x5E], 0xF8);
    AddB(Sc()[0x5F], 0xF8);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E4720: +2 through VenomRing_Phases (four entries: start, grow,
// MagicFx_CountDown9, BlizzardShard_End); a draw mode (tpage 0x35) committed;
// then, while +0 and +2 are set, the fan and the band.
S30_EXPORT void __cdecl VenomRing_Run(void) {
    Entry("VenomRing_Run", Addr(VenomRing_Phases), 4, Sc()[2])();
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(2, 0xC);
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::VenomRing_DrawFan);
    Call0(bof3::addr::VenomRing_DrawBand);
}

// original 0x4E4770: the ring at screen x +0xB x 26 + 0x98 (a word),
// y VenomRing_ScreenY[+0xB] + 0x50 (+0xB unbounded); radius +0x14 0x20, +9
// 0x3C, +0xA 0, +2 on.
S30_EXPORT void __cdecl VenomRing_Start(void) {
    const unsigned b = Sc()[0xB];
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, (b * 26 + 0x98) & 0xFFFF);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, (Word(Mem(Addr(VenomRing_ScreenY) + 2 * Sc()[0xB])) + 0x50) & 0xFFFF);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x20);
    Sc()[9] = 0x3C;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4E47D0: +0xA up; on at 0x10.
S30_EXPORT void __cdecl VenomRing_Grow(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4E47F0 (a phase of eight overlays' tables): +9 down; on at 0.
S30_EXPORT void __cdecl MagicFx_CountDown9(void) {
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E4810: a fan of eight gouraud triangles round the screen point
// (+0x2E, +0x30), radius +0x14; the centre's shade +0xA x 8, the rim's
// (+0xA x 8, x 4, x 8) - the scratch words +4..+0xA.
S30_EXPORT void __cdecl VenomRing_DrawFan(void) {
    {
        const unsigned char* const sc = Sc();
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
        SetSW(4, sc[0xA] << 3);
        SetSW(6, sc[0xA] << 3);
        SetSW(8, sc[0xA] << 2);
        SetSW(0xA, sc[0xA] << 3);
    }
    int s = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(s, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
    int c = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(c, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutFloat(p + 0x18, VS(0));
        PutFloat(p + 0x1C, VS(2));
        s = Sin(angle);
        SetVW(0, static_cast<unsigned>(Mul12(s, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
        PutFloat(p + 0x28, VS(0));
        c = Cos(angle);
        SetVW(2, static_cast<unsigned>(Mul12(c, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
        PutFloat(p + 0x2C, VS(2));
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = SB(6);
        p[0x15] = SB(8);
        p[0x16] = SB(0xA);
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(0xA);
        Commit(2, 0x34);
    }
}

// original 0x4E49F0: a band of eight gouraud quads round the same point
// between radius +0x14 and +0x14 x 2 + Rand & 7; the outer edge's shade 1,
// the inner's the fan's rim (scratch +6..+0xA).
S30_EXPORT void __cdecl VenomRing_DrawBand(void) {
    SetSW(0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_y));
    const std::uint32_t r = RandCall();
    SetSW(2, (r & 7) + (static_cast<unsigned>(Word(Sc() + 0x14)) << 1));
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
    v = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
    v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, VS(0));
        PutFloat(p + 0xC, VS(2));
        PutFloat(p + 0x28, VS(8));
        PutFloat(p + 0x2C, VS(0xA));
        v = Sin(angle);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
        PutFloat(p + 0x18, VS(0));
        v = Cos(angle);
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(2)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
        PutFloat(p + 0x1C, VS(2));
        v = Sin(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
        PutFloat(p + 0x38, VS(8));
        v = Cos(angle);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        PutFloat(p + 0x3C, VS(0xA));
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(0xA);
        p[0x34] = SB(6);
        p[0x35] = SB(8);
        p[0x36] = SB(0xA);
        Commit(2, 0x44);
    }
}

void VenomRing_Inject() {
    BOF3_INJECT(Venom_Task);
    BOF3_INJECT(Venom_Start);
    BOF3_INJECT(VenomChild_Task);
    BOF3_INJECT(VenomSprite_Run);
    BOF3_INJECT(VenomSprite_Start);
    BOF3_INJECT(VenomSprite_ShadeUp);
    BOF3_INJECT(VenomSprite_ShadeDown);
    BOF3_INJECT(VenomRing_Run);
    BOF3_INJECT(VenomRing_Start);
    BOF3_INJECT(VenomRing_Grow);
    BOF3_INJECT(MagicFx_CountDown9);
    BOF3_INJECT(VenomRing_DrawFan);
    BOF3_INJECT(VenomRing_DrawBand);
}

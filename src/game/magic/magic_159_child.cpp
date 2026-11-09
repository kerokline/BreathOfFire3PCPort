// Moved here by tools/move_funcs.py from magic_s34.cpp (docs/source-layout-round.md).
#include "game/magic/magic_159_child.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s34.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code), in the fuzz that
// address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// Linked at the sprite's (+0x34, +0x38), layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// A draw-mode packet (tpage 0x35, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EDDD0: the kind-2 task. A two-entry stack table by +1
// (Magic159_Start, BattleFx_Finish).
S34_EXPORT void __cdecl Magic159_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Magic159_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Magic159_Task");
}

// original 0x4EDE00: the task at the source sprite's point, +0xB 0, +1 on;
// one child (kind 1, 0x58) owned by this task, +1 0, +9 8, counted in +0xB;
// row 26 of the CLUT strip back with its STP bits; the target flagged 0x10;
// sound 0x100.
S34_EXPORT void __cdecl Magic159_Start(void) {
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = rv::As<records::SpriteHead>(src)->x;
        ts->z = rv::As<records::SpriteHead>(src)->z;
        rv::Store(ts->height, rv::As<records::SpriteHead>(src)->height);
        s[0xB] = 0;
        Inc(s[1]);
    }
    const unsigned slot = NewTask(0x58);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    child[9] = 8;
    Inc(self[0xB]);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    const unsigned char target = Mem(::bof3::Img(at::kTarget))[0];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Battle_SetTargetFlags)(target, 0x10);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4EDEC0: the child's kind-1 task, a jmp through
// Magic159Child_TaskTable (one entry) by +1, unchecked.
S34_EXPORT void __cdecl Magic159Child_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::Magic159Child_Run};
    Dispatch(kKinds, 1, Sc()[1], "Magic159Child_Task");
}

// original 0x4EDEE0: a call through Magic159Child_Steps (four) by +2; while
// +0 and +2 are set the screen point (BattleActor_UpdateScreenXY), x + 8,
// y - 0x10, and the quad (a tail jmp).
S34_EXPORT void __cdecl Magic159Child_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::Magic159Child_Wait, addr::Magic159Child_Grow,
                                                addr::Magic159Child_Hold, addr::Magic159Child_Fade};
    Dispatch(kSteps, 4, Sc()[2], "Magic159Child_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    AddWord(Sc() + 0x2E, 8);
    AddWord(Sc() + 0x30, 0xFFF0);
    Call0(addr::Magic159Child_Draw);
}

// original 0x4EDF30: +9 down; at 0 the child at the owner's point, +9 0x10,
// +0xA 0, +2 on.
S34_EXPORT void __cdecl Magic159Child_Wait(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    Dec(s[9]);
    if (s[9] != 0) return;
    ts->x = rv::As<records::SpriteHead>(Owner())->x;
    ts->z = rv::As<records::SpriteHead>(Owner())->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(Owner())->height);
    s[9] = 0x10;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4EDFA0: +9 up; +0xA up by 2, at 0x10 +2 on.
S34_EXPORT void __cdecl Magic159Child_Grow(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (s[0xA] == 0x10) Inc(s[2]);
}

// original 0x4EDFD0: +9 up; past 0x20 +2 on.
S34_EXPORT void __cdecl Magic159Child_Hold(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] > 0x20) Inc(s[2]);
}

// original 0x4EDFF0: +9 up; +0xA down by 2, at 0 the owner's count +0xB down
// and the task freed (a tail jmp).
S34_EXPORT void __cdecl Magic159Child_Fade(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EE030: a draw-mode packet (tpage 0x35); one semi-transparent
// textured gouraud quad (Gpu_SetPolyGT4) round the screen point (+0x2E,
// +0x30) at radius +9 x 2, its corners at the angles 0xE00, 0xA00, 0x200,
// 0x600, page (0x340, 0x100), CLUT row 0x1FA, the texture's (8, 8)..(0x57,
// 0x48), shaded +0xA x 8 on the first two corners and +0xA on the last two;
// linked at the child, layer 2.
S34_EXPORT void __cdecl Magic159Child_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* p;
    {
        const unsigned char* const s = Sc();
        p = Gfx_PacketNext;
        SetSW(0, s[9] * 2u);
        SetSW(2, s[0xA] * 8u);
        SetSW(4, s[0xA]);
    }
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0xE00, 0xA00, 0x200, 0x600};
    static constexpr unsigned kAt[4] = {8, 0x1C, 0x30, 0x44};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        PutFloat(p + kAt[c], Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = Sin(kAngles[c]);
        PutFloat(p + kAt[c] + 4, Mul12(v, SS(0)) + S16(Sc() + 0x30));
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100) & 0xFFFF);
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA) & 0xFFFF);
    p[0x14] = 8;
    p[0x15] = 8;
    p[0x29] = 8;
    p[0x3C] = 8;
    p[0x28] = 0x57;
    p[0x3D] = 0x48;
    p[0x50] = 0x57;
    p[0x51] = 0x48;
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = SB(2);
    for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(4);
    LinkAtSprite(0x54);
}

void Magic159Child_Inject() {
    BOF3_INJECT(Magic159_Task);
    BOF3_INJECT(Magic159_Start);
    BOF3_INJECT(Magic159Child_Task);
    BOF3_INJECT(Magic159Child_Run);
    BOF3_INJECT(Magic159Child_Wait);
    BOF3_INJECT(Magic159Child_Grow);
    BOF3_INJECT(Magic159Child_Hold);
    BOF3_INJECT(Magic159Child_Fade);
    BOF3_INJECT(Magic159Child_Draw);
}

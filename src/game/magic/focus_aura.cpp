// Moved here by tools/move_funcs.py from magic_s07.cpp (docs/source-layout-round.md).
#include "game/magic/focus_aura.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s07.h"
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

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

constexpr ::bof3::image::Va kAbilityId{0x904B80};       // u16: the ability id (Battle_StartItemMagic stores it)

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `add r32, r32`: the 32-bit sum, wrapped.
std::int32_t Sum(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The projections with the arguments the originals push: the depth and a
// flag pointer past the declared ones (cdecl: the caller pops them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S07_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer `dy`.
void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's position (+0x34 / +0x38 / +0x3C) the owner's, each read again.
void PositionFromOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// Row 26 of Gfx_ClutStrip, its 32 words, back from its source.
void RestoreRow26() {
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
    }
}

}  // namespace

#define S07_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The ring's shades, words 0x903850 / 52 / 54: FocusAura_RingShades[+4]
// (or FocusAura_DiscShades) x +9 below 0x10, else x 16.
void Shades(std::uint32_t table) {
    const unsigned char* const s = Sc();
    const unsigned k = s[4] * 3u;
    if (s[9] < 0x10) {
        SetSW(0, Mem(table + k)[0] * static_cast<unsigned>(s[9]));
        SetSW(2, Mem(table + 1 + k)[0] * static_cast<unsigned>(s[9]));
        SetSW(4, Mem(table + 2 + k)[0] * static_cast<unsigned>(s[9]));
    } else {
        SetSW(0, static_cast<unsigned>(Mem(table + k)[0]) << 4);
        SetSW(2, static_cast<unsigned>(Mem(table + 1 + k)[0]) << 4);
        SetSW(4, static_cast<unsigned>(Mem(table + 2 + k)[0]) << 4);
    }
}

}  // namespace

// original 0x4A4C10: the kind-2 task. A three-entry stack table by +1:
// Focus_Start, MAGIC009's 0x49DA50 (+1 on at +0xB 0), BattleFx_Finish.
S07_EXPORT void __cdecl Focus_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Focus_Start, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Focus_Task");
}

// original 0x4A4C40: the position the owner's; +4 Focus_Kind (2, or 3 for
// ability 0xA3); +0xB 0, +1 on; the aura (kind 1, 0x3B): +0x80 this task
// (read after the create), +1 0, +4 this task's +4 - 2; then eight motes, the
// same with +1 1, +0xB the number, +9 FocusMote_Delays[i] + 9; +0xB up for
// each. Row 26 of the CLUT strip back; Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl Focus_Start(void) {
    PositionFromOwner();
    const unsigned char kind = MH_AT(ByteFn, bof3::addr::Focus_Kind)();
    Sc()[4] = kind;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x3B);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(self[4] - 2);
        Inc(self[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x3B);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 1;
        child[4] = static_cast<unsigned char>(self[4] - 2);
        const auto delay = static_cast<unsigned char>(Mem(::bof3::Img(FocusMote_Delays_addr) + i)[0] + 9);
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = delay;
        Inc(self[0xB]);
    }
    RestoreRow26();
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A4D90: 3 when the ability word 0x904B80 is 0xA3, else 2, in al
// (the rest of eax the caller's).
S07_EXPORT unsigned char __cdecl Focus_Kind(void) { return Word(Mem(::bof3::Img(kAbilityId.v))) == 0xA3 ? 3 : 2; }

// original 0x4A4DA0: the children's kind-1 task, a jmp through
// FocusChild_Kinds (two entries: the aura, a mote) by +1, unchecked.
S07_EXPORT void __cdecl FocusChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::FocusAura_Run, bof3::addr::FocusMote_Run};
    Dispatch(kKinds, 2, Sc()[1], "FocusChild_Task");
}

// original 0x4A4DC0: a six-entry stack table by +2 - _Start, _Grow, _Swell,
// _Brighten, _Dim, _End - then with +0 bit 0 and +2 not 0: the actor matrix
// pushed, the ring and the disc, the matrix popped.
S07_EXPORT void __cdecl FocusAura_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::FocusAura_Start,    bof3::addr::FocusAura_Grow,
                                                bof3::addr::FocusAura_Swell,    bof3::addr::FocusAura_Brighten,
                                                bof3::addr::FocusAura_Dim,      bof3::addr::FocusAura_End};
    Dispatch(kSteps, 6, Sc()[2], "FocusAura_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::FocusAura_DrawRing);
    Call0(bof3::addr::FocusAura_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A4E30: the position the owner's; +9 0, +0xA 1, +2 on.
S07_EXPORT void __cdecl FocusAura_Start(void) {
    PositionFromOwner();
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4A4E80: +9 up by 2; at 0x10 +0xA 0 and +2 on.
S07_EXPORT void __cdecl FocusAura_Grow(void) {
    AddB(Sc()[9], 2);
    unsigned char* const s = Sc();
    if (s[9] != 0x10) return;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4A4EB0: +9 up, +0xA up by 2; at 0x18 the source sprite's tint
// released and a black one set (Sprite_SetTint(source, 0, 0, 0, 1), the
// source read again), its slot to +0xB; +2 on.
S07_EXPORT void __cdecl FocusAura_Swell(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 2);
    if (Sc()[0xA] != 0x18) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Inc(Sc()[2]);
}

// original 0x4A4F10: +9 up; the tint record +0xB (MoveScript_TintRecords + 12
// n) up by 2 in its three channels (+2..+4); at channel +2 0x10 +2 on.
S07_EXPORT void __cdecl FocusAura_Brighten(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(MoveScript_TintRecords[s[0xB] * 12u + c], 2);
    if (MoveScript_TintRecords[s[0xB] * 12u + 2] == 0x10) Inc(s[2]);
}

// original 0x4A4F90: +9 up; the tint record's three channels down by 2; at
// channel +2 0 +2 on.
S07_EXPORT void __cdecl FocusAura_Dim(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(MoveScript_TintRecords[s[0xB] * 12u + c], 0xFE);
    if (MoveScript_TintRecords[s[0xB] * 12u + 2] == 0) Inc(s[2]);
}

// original 0x4A5010: +9 up, +0xA down by 2; at 0 the source's tint released,
// the target flashed, the owner's +0xB down, the task freed (a tail jump).
S07_EXPORT void __cdecl FocusAura_End(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5060: a three-entry stack table by +2 - _Start, _Grow, _Rise -
// then with +0 bit 0 and +2 not 0 the screen point and the mote's quad.
S07_EXPORT void __cdecl FocusMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::FocusMote_Start, bof3::addr::FocusMote_Grow,
                                                bof3::addr::FocusMote_Rise};
    Dispatch(kSteps, 3, Sc()[2], "FocusMote_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::FocusMote_Draw);
}

// original 0x4A50B0: +9 down; at 0 the angle +0xB sl 9 (word 0x90385A, read
// again for the sin); the position the owner's + cos / sin x 3 sl 2; +9 0,
// +0xA 4, +2 on.
S07_EXPORT void __cdecl FocusMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned angle = (static_cast<unsigned>(Sc()[0xB]) << 9) & 0xFFFF;
    SetSW(0xA, angle);
    int v = MH_CALL(Math_Cos)(static_cast<short>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Sum(Shl(Imul(v, 3), 0xE) >> 12, rv::As<records::SpriteHead>(Owner())->x));
    v = MH_CALL(Math_Sin)(SS(0xA));
    rv::Store(rv::Current<records::BattleTask>()->z, Sum(Shl(Imul(v, 3), 0xE) >> 12, rv::As<records::SpriteHead>(Owner())->z));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4A5160: +9 up by 0x10; at 0x80 +2 on.
S07_EXPORT void __cdecl FocusMote_Grow(void) {
    AddB(Sc()[9], 0x10);
    if (Sc()[9] == 0x80) Inc(Sc()[2]);
}

// original 0x4A5180: +9 up by 8, +0xA down; at 0 the owner's +0xB down and the
// task freed (a tail jump).
S07_EXPORT void __cdecl FocusMote_Rise(void) {
    AddB(Sc()[9], 8);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A51C0: one semi-transparent gouraud quad over the screen point,
// half-width +0xA and height +9 above it (scratch words 0x903858 / 0x903856,
// the point at 0x90385A / 0x90385C), shaded 1 at the top and
// FocusMote_Shades[+4] (words 0x903850 / 52 / 54) at the bottom; linked at
// the task on layer 0 behind a draw mode (tpage 0x35).
S07_EXPORT void __cdecl FocusMote_Draw(void) {
    const unsigned char* const s = Sc();
    SetSW(6, s[9]);
    SetSW(8, s[0xA]);
    SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xC, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    const unsigned k = s[4] * 3u;
    SetSW(0, Mem(::bof3::Img(FocusMote_Shades_addr) + k)[0]);
    SetSW(2, Mem(::bof3::Img(FocusMote_Shades_addr) + 1 + k)[0]);
    SetSW(4, Mem(::bof3::Img(FocusMote_Shades_addr) + 2 + k)[0]);
    DrawMode(0x35);
    LinkAtSprite(0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xA) - SS(8));
    PutFloat(p + 0xC, SS(0xC) - SS(6));
    PutFloat(p + 0x18, SS(0xA) + SS(8));
    PutFloat(p + 0x1C, SS(0xC) - SS(6));
    PutFloat(p + 0x28, SS(0xA) - SS(8));
    PutFloat(p + 0x2C, SS(0xC));
    PutFloat(p + 0x38, SS(0xA) + SS(8));
    PutFloat(p + 0x3C, SS(0xC));
    for (unsigned c : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[c] = 1;
    p[0x24] = SB(0);
    p[0x25] = SB(2);
    p[0x26] = SB(4);
    p[0x34] = SB(0);
    p[0x35] = SB(2);
    p[0x36] = SB(4);
    LinkAtSprite(0, 0x44);
}

// original 0x4A53A0: the aura's ring, 64 semi-transparent gouraud quads under
// the actor matrix. Height word 0x903858 = +0xA; each step an angle
// ((Rand & 3) + +9 + step) & 0xF sl 8 gives the inner lift (0x903856) and
// radius (0x90385E), the outer radius 0x80 (0x90385C). Each quad: the last
// inner and outer points and the new ones at angle step x 0x40 (vertices
// 0x9037A0..B8), projected (Gte_RotTransPers4), depth-sorted, and linked at
// the task's +0x34 / +0x38 each moved by the new outer x sl 9 (both by x) on
// layer 0; shaded 1 inside, FocusAura_RingShades outside.
S07_EXPORT void __cdecl FocusAura_DrawRing(void) {
    SetSW(8, Sc()[0xA]);
    std::uint32_t r = RandCall();
    unsigned angle = (((r & 3) + Sc()[9]) & 0xF) << 8;
    SetSW(0xA, angle);
    {
        int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int inner = Shl(Imul(v, SS(8)), 1) >> 12;
        const short again = SS(0xA);
        const std::uint32_t lift = static_cast<std::uint32_t>(inner) - 6u * static_cast<std::uint32_t>(SD(8));
        SetSW(0xC, 0x80);
        SetSW(6, lift);
        v = MH_CALL(Math_Sin)(again);
        const int outer = Shl(Imul(v, SS(8)), 2) >> 12;
        SetSW(0xE, 8u * static_cast<std::uint32_t>(SD(8)) + 0x80u - static_cast<std::uint32_t>(outer));
    }
    Shades(::bof3::Img(FocusAura_RingShades_addr));
    int v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0xE))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0xE))));
    SetVW(0xC, SW(6));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0xC))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x14, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0xC))));
    SetVW(0x1C, 0);
    for (unsigned step = 1, e = 0x40; e < 0x1040; ++step, e += 0x40) {
        r = RandCall();
        angle = (((r & 3) + Sc()[9] + step) & 0xF) << 8;
        SetSW(0xA, angle);
        v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int inner = Shl(Imul(v, SS(8)), 1) >> 12;
        const short again = SS(0xA);
        SetSW(6, static_cast<std::uint32_t>(inner) - 6u * static_cast<std::uint32_t>(SD(8)));
        v = MH_CALL(Math_Sin)(again);
        const int outer = Shl(Imul(v, SS(8)), 2) >> 12;
        const std::uint32_t radius = 8u * static_cast<std::uint32_t>(SD(8)) + 0x80u - static_cast<std::uint32_t>(outer);
        const std::uint16_t y1 = VW(0xA), x1 = VW(8);
        SetSW(0xE, radius);
        const std::uint16_t z1 = VW(0xC);
        SetVW(0, x1);
        SetVW(2, y1);
        SetVW(4, z1);
        v = MH_CALL(Math_Cos)(static_cast<int>(e));
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0xE))));
        v = MH_CALL(Math_Sin)(static_cast<int>(e));
        const int y = Mul12(v, SS(0xE));
        const std::uint16_t x3 = VW(0x18), y3 = VW(0x1A);
        SetVW(0xA, static_cast<unsigned>(y));
        SetVW(0xC, SW(6));
        SetVW(0x10, x3);
        SetVW(0x12, y3);
        v = MH_CALL(Math_Cos)(static_cast<int>(e));
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0xC))));
        v = MH_CALL(Math_Sin)(static_cast<int>(e));
        const int y4 = Mul12(v, SS(0xC));
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        SetVW(0x1A, static_cast<unsigned>(y4));
        const std::uint32_t shift = static_cast<std::uint32_t>(Shl(VS(0x18), 9));
        const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->x) + shift);
        const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->z) + shift);
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned c : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[c] = 1;
        p[0x24] = SB(0);
        p[0x25] = SB(2);
        p[0x26] = SB(4);
        p[0x34] = SB(0);
        p[0x35] = SB(2);
        p[0x36] = SB(4);
        long depth, flag;
        S07_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38, &depth,
                                         &flag);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
}

// original 0x4A57C0: the aura's disc, 16 semi-transparent gouraud triangles
// under the actor matrix: the centre (0, 0, 0) and two rim points of radius
// 0x88 (word 0x903856) 0x100 apart, projected (Gte_RotTransPers3),
// depth-sorted and committed to layer 5; shaded 1 at the centre,
// FocusAura_DiscShades at the rim; between draw modes (tpage 0x35, then
// 0x15).
S07_EXPORT void __cdecl FocusAura_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSW(6, 0x88);
    Shades(::bof3::Img(FocusAura_DiscShades_addr));
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(6))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(6))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int e = 0x100; e < 0x1100; e += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x2 = VW(0x10), y2 = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x2);
        SetVW(0xA, y2);
        v = MH_CALL(Math_Sin)(e);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(6))));
        v = MH_CALL(Math_Cos)(e);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(6))));
        long depth, flag;
        S07_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(0);
        p[0x15] = SB(2);
        p[0x16] = SB(4);
        p[0x24] = SB(0);
        p[0x25] = SB(2);
        p[0x26] = SB(4);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void FocusAura_Inject() {
    BOF3_INJECT(Focus_Task);
    BOF3_INJECT(Focus_Start);
    BOF3_INJECT(Focus_Kind);
    BOF3_INJECT(FocusChild_Task);
    BOF3_INJECT(FocusAura_Run);
    BOF3_INJECT(FocusAura_Start);
    BOF3_INJECT(FocusAura_Grow);
    BOF3_INJECT(FocusAura_Swell);
    BOF3_INJECT(FocusAura_Brighten);
    BOF3_INJECT(FocusAura_Dim);
    BOF3_INJECT(FocusAura_End);
    BOF3_INJECT(FocusMote_Run);
    BOF3_INJECT(FocusMote_Start);
    BOF3_INJECT(FocusMote_Grow);
    BOF3_INJECT(FocusMote_Rise);
    BOF3_INJECT(FocusMote_Draw);
    BOF3_INJECT(FocusAura_DrawRing);
    BOF3_INJECT(FocusAura_DrawDisc);
}

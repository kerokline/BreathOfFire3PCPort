// Moved here by tools/move_funcs.py from magic_s07.cpp (docs/source-layout-round.md).
#include "game/magic/enlighten_rays.h"

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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn2 = void (__cdecl*)(unsigned, unsigned);

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

// original 0x4A5A10: the kind-2 task. A three-entry stack table by +1:
// Enlighten_Start, _Apply, MagicFx_EndWhenChildrenDone.
S07_EXPORT void __cdecl Enlighten_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Enlighten_Start, bof3::addr::Enlighten_Apply,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 3, Sc()[1], "Enlighten_Task");
}

// original 0x4A5A40: the owner's direction +8 and position; +0xB 0, +1 on;
// two children (kind 1, 0x3C): the rays (+1 0) and the ring (+1 1), each
// +0x80 this task (read after the create) and +0xB up. Row 26 of the CLUT
// strip back; Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl Enlighten_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned kind = 0; kind < 2; ++kind) {
        const unsigned slot = NewTask(0x3C);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = static_cast<unsigned char>(kind);
        Inc(self[0xB]);
    }
    RestoreRow26();
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A5B50: once +0xB (the children alive) is 0: the buff
// (MagicFx_ApplyBuff(3, the target byte)) and its popup (kind 1, 0x48, the
// BuffPopup task): +0x80 this task (read after the create), +4 its icon (3,
// or 8 when resisted), +9 1, +0xA 0; +0xB up, +1 on.
S07_EXPORT void __cdecl Enlighten_Apply(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned char applied = MH_CALL(MagicFx_ApplyBuff)(3, TargetByte());
    const unsigned slot = NewTask(0x48);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[4] = applied != 0 ? 3 : 8;
    child[9] = 1;
    child[0xA] = 0;
    Inc(self[0xB]);
    Inc(Sc()[1]);
}

// original 0x4A5C10: the children's kind-1 task, a jmp through
// EnlightenChild_Kinds (two entries: the rays, the ring) by +1, unchecked.
S07_EXPORT void __cdecl EnlightenChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::EnlightenRays_Run, bof3::addr::EnlightenRing_Run};
    Dispatch(kKinds, 2, Sc()[1], "EnlightenChild_Task");
}

// original 0x4A5C30: a four-entry stack table by +2 - _Start, _Grow, group
// S20's Magic088_WaveGrow, _Fade - then with +0 bit 0 and +2 not 0: the disc,
// and the lines twice: from angle step +9 at length +0xA x 3, from +9 + 4 at
// +0xA x 2 (each byte of the task read again after the call before).
S07_EXPORT void __cdecl EnlightenRays_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::EnlightenRays_Start, bof3::addr::EnlightenRays_Grow,
                                                bof3::addr::Magic088_WaveGrow, bof3::addr::EnlightenRays_Fade};
    Dispatch(kSteps, 4, Sc()[2], "EnlightenRays_Run");
    const unsigned char* s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::EnlightenRays_DrawDisc);
    s = Sc();
    MH_AT(Fn2, bof3::addr::EnlightenRays_DrawLines)(s[9], s[0xA] * 3u);
    s = Sc();
    MH_AT(Fn2, bof3::addr::EnlightenRays_DrawLines)(static_cast<unsigned char>(s[9] + 4), s[0xA] * 2u);
}

// original 0x4A5CB0: the owner's direction and position; anchored on the
// target (Enlighten_AnchorToActor); +9 and +0xA 0, +2 on.
S07_EXPORT void __cdecl EnlightenRays_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Call0(bof3::addr::Enlighten_AnchorToActor);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A5D20: +9 up, +0xA up; at +0xA 0x10 +2 on.
S07_EXPORT void __cdecl EnlightenRays_Grow(void) {
    Inc(Sc()[9]);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4A5D50: +9 up, +0xA down; at 0 the owner's +0xB down and the
// task freed (a tail jump).
S07_EXPORT void __cdecl EnlightenRays_Fade(void) {
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5D90: a two-entry stack table by +2 - _Start, _Spread - then
// with +0 bit 0 and +2 not 0 the ring.
S07_EXPORT void __cdecl EnlightenRing_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::EnlightenRing_Start, bof3::addr::EnlightenRing_Spread};
    Dispatch(kSteps, 2, Sc()[2], "EnlightenRing_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::EnlightenRing_Draw);
}

// original 0x4A5DD0: the owner's direction and position; anchored on the
// target; +0xB 0xB, +9 0, +0xA 0x80, +2 on.
S07_EXPORT void __cdecl EnlightenRing_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Call0(bof3::addr::Enlighten_AnchorToActor);
    Sc()[0xB] = 0xB;
    Sc()[9] = 0;
    Sc()[0xA] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4A5E40: +0xB down, +9 up by it, +0xA down by 0xC; at +0xB 0 the
// owner's +0xB down and the task freed (a tail jump).
S07_EXPORT void __cdecl EnlightenRing_Spread(void) {
    Dec(Sc()[0xB]);
    unsigned char* const s = Sc();
    AddB(s[9], s[0xB]);
    AddB(s[0xA], 0xF4);
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5E90: the task's screen point (BattleActor_UpdateScreenXY);
// then for a party target (the target byte below 3) moved by that member's
// offset: the index 0x904B89 through Enlighten_AnchorIndexB into
// Enlighten_AnchorOffsetsB while the record's +0x134 has bit 1, else its
// +0x89 into Enlighten_AnchorOffsets; the pair (+8 >> 1) + index x 2: dx
// added to word +0x2E for a direction 0 or 3, else taken off; dy added to
// +0x30.
S07_EXPORT void __cdecl Enlighten_AnchorToActor(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned target = TargetByte();
    if (target >= 3) return;
    const unsigned char* const rec = PartyRecord(target);
    unsigned index;
    std::uint32_t table;
    if (rec[0x134] & 2) {
        index = Mem(::bof3::Img(Enlighten_AnchorIndexB_addr) + Mem(::bof3::Img(BattlePhases_B89_addr))[0])[0];
        table = ::bof3::Img(Enlighten_AnchorOffsetsB_addr);
    } else {
        index = rec[0x89];
        table = ::bof3::Img(Enlighten_AnchorOffsets_addr);
    }
    unsigned char* const s = Sc();
    const unsigned facing = s[8];
    const std::uint16_t dx = Word(Mem(table + ((facing >> 1) + index * 2) * 4));
    if (facing == 0 || facing == 3) rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, (static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) + dx) & 0xFFFF);
    else rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, (static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) - dx) & 0xFFFF);
    unsigned char* const t = Sc();
    const std::uint16_t dy = Word(Mem(table + 2 + ((t[8] >> 1) + index * 2) * 4));
    rv::Store<unsigned>(rv::As<records::BattleTask>(t)->screen_y, (static_cast<std::uint16_t>(rv::As<records::BattleTask>(t)->screen_y) + dy) & 0xFFFF);
}

// original 0x4A5FC0: the rays: from the screen point (+0x2E / +0x30, kept in
// words 0x903850 / 52) semi-transparent gouraud lines at angle (step & 0x1F)
// sl 7 (word 0x903858, read again for each call) and length `length` (its low
// word), for step = `first` (a byte), + 8, .. while the byte step stays below
// first + 0x20 - four lines; for a first of 0xE0 or more the byte never gets
// there and the loop does not end (the original's; section 7 of the doc).
// Shaded (0x80, 0x70, 0x60) at the point, 1 at the end; committed to layer 3
// between draw modes (tpage 0x35, then 0x15).
S07_EXPORT void __cdecl EnlightenRays_DrawLines(unsigned first, unsigned length) {
    const unsigned char* const s = Sc();
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    auto step = static_cast<unsigned char>(first);
    const unsigned end = static_cast<unsigned>(step) + 0x20;
    const int len = static_cast<int>(length & 0xFFFF);
    do {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const short x = SS(0);
        SetSW(8, static_cast<unsigned>(step & 0x1F) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, len) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, len) + SS(2));
        p[4] = 0x80;
        p[5] = 0x70;
        p[6] = 0x60;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
        step = static_cast<unsigned char>(step + 8);
    } while (step < end);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A6120: a disc of 16 semi-transparent gouraud triangles round the
// screen point, radius +0xA (word 0x903854), 0x200 apart; shaded 0x80 at the
// centre, 1 at the rim; committed to layer 3 between draw modes (tpage 0x35,
// then 0x15).
S07_EXPORT void __cdecl EnlightenRays_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, s[0xA]);
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    for (int e = 0; e < 0x2000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Sin)(e);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(e);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        e += 0x200;
        v = MH_CALL(Math_Sin)(e);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(e);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        for (unsigned c : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[c] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A62B0: a ring of 32 semi-transparent flat lines round the
// screen point, radius +9 (word 0x903854), 0x80 apart (word 0x903858, read
// again for each cos), shaded +0xA grey (word 0x90385A); committed to layer 3
// between draw modes (tpage 0x35, then 0x15).
S07_EXPORT void __cdecl EnlightenRing_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* const s = Sc();
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    SetSW(0xA, s[0xA]);
    SetSW(4, s[9]);
    for (unsigned i = 0; i < 0x20; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        unsigned angle = (i << 7) & 0xFFFF;
        SetSW(8, angle);
        int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        PutFloat(p + 8, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0xC, Mul12(v, SS(4)) + SS(2));
        angle = ((i + 1) << 7) & 0xFFFF;
        SetSW(8, angle);
        v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        PutFloat(p + 0x14, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(2));
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(3, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

void EnlightenRays_Inject() {
    BOF3_INJECT(Enlighten_Task);
    BOF3_INJECT(Enlighten_Start);
    BOF3_INJECT(Enlighten_Apply);
    BOF3_INJECT(EnlightenChild_Task);
    BOF3_INJECT(EnlightenRays_Run);
    BOF3_INJECT(EnlightenRays_Start);
    BOF3_INJECT(EnlightenRays_Grow);
    BOF3_INJECT(EnlightenRays_Fade);
    BOF3_INJECT(EnlightenRing_Run);
    BOF3_INJECT(EnlightenRing_Start);
    BOF3_INJECT(EnlightenRing_Spread);
    BOF3_INJECT(Enlighten_AnchorToActor);
    BOF3_INJECT(EnlightenRays_DrawLines);
    BOF3_INJECT(EnlightenRays_DrawDisc);
    BOF3_INJECT(EnlightenRing_Draw);
}

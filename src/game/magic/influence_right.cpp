// Moved here by tools/move_funcs.py from magic_s15.cpp (docs/source-layout-round.md).
#include "game/magic/influence_right.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s15.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The height tables InfluenceMark_Start lifts its quad by: a party member's
// (two bytes by its +0x89, the first when its +8 is 0 or 1) and an enemy
// type's (+0x8C x the type +0xF0; group S20's lift bytes).
constexpr ::bof3::image::Va kPartyLift{0x64DFC8};

constexpr ::bof3::image::Va kEnemyLift{0x8C564F};

constexpr std::uint32_t kEnemyLiftStride = 0x8C;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

}  // namespace

#define S15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The actor +0xB names: a member's record for 0..2, else the enemy's (by the
// index - 3, unchecked).
unsigned char* MarkedRecord(unsigned index) { return index < 3 ? PartyRecord(index) : EnemyRecord(index); }

// The shade the ring and the triangles share: 0x80 - 48 x +0xB before step 2.
unsigned FadeShade(const unsigned char* s) { return 0x80u - static_cast<unsigned>(s[0xB]) * 0x30u; }

}  // namespace

// original 0x4B7DE0: the kind-2 task. A two-entry stack table by +1:
// Influence_Start, MAGIC222's BattleFx_Finish.
S15_EXPORT void __cdecl Influence_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Influence_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Influence_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B7E10: the source sprite's screen point (0x904B4C +0x2E /
// +0x30); +0xB 0, +1 on; three children (kind 1, 0x39) with +1 0 and three
// with +1 1, each with +0x80 this task, +0xB its number 0..2 and +9 one more,
// counted in +0xB; the first 48 words of CLUT row 26 back from the source; sound
// 0x100. BattleTask_Create's 0xFF is not tested.
S15_EXPORT void __cdecl Influence_Start(void) {
    {
        const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(source)->screen_x));
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(source)->screen_y));
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned side = 0; side < 2; ++side) {
        for (unsigned n = 0; n < 3; ++n) {
            const unsigned slot = NewTask(0x39);
            unsigned char* const child = TaskSlot(slot);
            records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
            unsigned char* const s = Sc();
            tchild->owner = static_cast<std::int32_t>(Key(s));
            child[1] = static_cast<unsigned char>(side);
            child[0xB] = static_cast<unsigned char>(n);
            child[9] = static_cast<unsigned char>(n + 1);
            Inc(s[0xB]);
        }
    }
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
        Gfx_ClutStrip[0x1A20 + k] = Gfx_ClutStripSource[0x1A20 + k];
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4B7F40: the children's kind-1 task, a jmp through
// InfluenceChild_Kinds (three entries: the right-hand ones, the left-hand ones,
// the markers) by +1, unchecked.
S15_EXPORT void __cdecl InfluenceChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::InfluenceRight_Run, bof3::addr::InfluenceLeft_Run,
                                                bof3::addr::InfluenceMark_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("InfluenceChild_Task", phase, 3);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B7F60: a call through InfluenceRight_Steps (five entries) by
// +2; a draw-mode packet (tpage 0x35) to layer 3; with +0 bit 0 set: past step
// 0 the ring and the triangles, past step 2 the cross; a closing packet
// (0x15).
S15_EXPORT void __cdecl InfluenceRight_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::InfluenceRight_Launch, bof3::addr::InfluenceRight_Slide,
                                                bof3::addr::InfluenceRight_Pick, bof3::addr::InfluenceRight_Blink,
                                                bof3::addr::InfluenceRight_Shrink};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("InfluenceRight_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    DrawModeCommit(0x35, 3);
    const unsigned char* s = Sc();
    if (s[0] & 1) {
        if (s[2] != 0) {
            Call0(bof3::addr::Influence_DrawRing);
            Call0(bof3::addr::Influence_DrawTriangles);
            s = Sc();
        }
        if ((s[0] & 1) && s[2] > 2) Call0(bof3::addr::Influence_DrawCross);
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B7FF0: +9 down; at 0 the slide +0xC 0xB (a dword), the screen
// point 0x37 right of the owner's and 0x10 up, +9 0, +0xA 0x20, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Current<records::BattleTask>()->step_x = 0xB;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x) + 0x37u);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(Owner() + 0x30) - 0x10u);
    Sc()[9] = 0;
    Sc()[0xA] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4B8060: the slide +0xC down by 1 and the x (word +0x2E) left by
// it; at 0 the first child (+0xB 0) plays sound 0x103 and goes on, the others
// count the owner's +0xB down and free themselves.
S15_EXPORT void __cdecl InfluenceRight_Slide(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) - 1u));
    {
        unsigned char* const s = Sc();
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) - static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
    }
    if (rv::Current<records::BattleTask>()->step_x != 0) return;
    if (Sc()[0xB] == 0) {
        MH_CALL(Sound_PlayById)(0x103);
        Inc(Sc()[2]);
        return;
    }
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B80B0: +9 up; at 0x10 the markers spawned
// (Influence_SpawnMarks), +0xB 0, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Pick(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    Call0(bof3::addr::Influence_SpawnMarks);
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
}

// original 0x4B80E0: +9 swings - up by 4 to 0x10 while +0xB is set (then +0xB
// 0), down by 2 to 0 while it is clear (then +0xB 1); once the owner's count
// +0xB is 1 (only this child left): sound 0x101, +9 0x10, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Blink(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 0) {
        AddB(s[9], 4);
        if (Sc()[9] == 0x10) Sc()[0xB] = 0;
    } else {
        AddB(s[9], 0xFE);
        if (Sc()[9] == 0) Sc()[0xB] = 1;
    }
    if (Owner()[0xB] != 1) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4B8150: the ring +0xA down by 2; at 8 the owner's +0xB down and
// the task freed.
S15_EXPORT void __cdecl InfluenceRight_Shrink(void) {
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 8) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B8180: a call through InfluenceLeft_Steps (two entries) by +2;
// a draw-mode packet (tpage 0x35) to layer 3; with +0 bit 0 set and past step
// 0 the ring and the triangles; a closing packet (0x15).
S15_EXPORT void __cdecl InfluenceLeft_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::InfluenceLeft_Launch, bof3::addr::InfluenceLeft_Slide};
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("InfluenceLeft_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
    DrawModeCommit(0x35, 3);
    const unsigned char* const s = Sc();
    if ((s[0] & 1) && s[2] != 0) {
        Call0(bof3::addr::Influence_DrawRing);
        Call0(bof3::addr::Influence_DrawTriangles);
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B81F0: as InfluenceRight_Launch, 0x37 left of the owner's point.
S15_EXPORT void __cdecl InfluenceLeft_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Current<records::BattleTask>()->step_x = 0xB;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x) - 0x37u);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(Owner() + 0x30) - 0x10u);
    Sc()[9] = 0;
    Sc()[0xA] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4B8260: the slide +0xC down by 1 and the x right by it; at 0 the
// owner's +0xB down and the task freed.
S15_EXPORT void __cdecl InfluenceLeft_Slide(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) - 1u));
    {
        unsigned char* const s = Sc();
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
    }
    if (rv::Current<records::BattleTask>()->step_x != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B82A0: the marker's steps, a jmp through InfluenceMark_Steps
// (five entries: _Start, _Brighten, _Fade, _Show, MAGIC058's 0x4AF490) by +2,
// unchecked.
S15_EXPORT void __cdecl InfluenceMark_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::InfluenceMark_Start, bof3::addr::InfluenceMark_Brighten,
                                                bof3::addr::InfluenceMark_Fade, bof3::addr::InfluenceMark_Show,
                                                bof3::addr::MagicFx_UncountAndFree};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("InfluenceMark_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B82C0: the actor's sprite (+0xB: a member for 0..2, else an
// enemy) tinted black, its tint slot to +0xA; the screen point 0x20 left of and
// 0xC above the sprite's, then raised by its height byte (a member's
// kPartyLift pair by +0x89, the first for +8 0 or 1; an enemy type's lift byte);
// +9 0, +2 on.
S15_EXPORT void __cdecl InfluenceMark_Start(void) {
    unsigned char* const sprite = MarkedRecord(Sc()[0xB]);
    MH_CALL(Sprite_ReleaseTint)(sprite);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(sprite, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, Word(sprite + 0x2E) - 0x20u);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(sprite + 0x30) - 0xCu);
    unsigned char* const s = Sc();
    const unsigned index = s[0xB];
    unsigned lift;
    if (index < 3) {
        const unsigned char* const r = PartyRecord(index);
        lift = Mem(::bof3::Img(kPartyLift.v) + 2u * r[0x89] + (r[8] <= 1 ? 0u : 1u))[0];
    } else {
        lift = Mem(::bof3::Img(kEnemyLift.v) + EnemyRecord(index)[0xF0] * kEnemyLiftStride)[0];
    }
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_y, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y) - lift);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4B83D0: the tint record +0xA (MoveScript_TintRecords + 12 n) one
// step brighter in each of its three channels (+2..+4); at 0x10 +2 on.
S15_EXPORT void __cdecl InfluenceMark_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    if (MoveScript_TintRecords[s[0xA] * 12u + 2] == 0x10) Inc(s[2]);
}

// original 0x4B8440: the tint record one step darker; at 0: sound 0x102 for
// the first marker (+4 0), the tint released (Tint_Release), the actor +0xB
// flashed, +9 0, +0xA 4, +2 on.
S15_EXPORT void __cdecl InfluenceMark_Fade(void) {
    {
        unsigned char* const s = Sc();
        for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
        if (MoveScript_TintRecords[s[0xA] * 12u + 2] != 0) return;
        if (s[4] == 0) MH_CALL(Sound_PlayById)(0x102);
    }
    MH_CALL(Tint_Release)(Sc()[0xA]);
    MH_CALL(BattleActor_Flash)(Sc()[0xB]);
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4B84F0: the quad drawn; its height +0xA up by 4 to 0x18; +9 up;
// at 0x14 +2 on.
S15_EXPORT void __cdecl InfluenceMark_Show(void) {
    Call0(bof3::addr::InfluenceMark_Draw);
    unsigned char* const s = Sc();
    if (s[0xA] != 0x18) AddB(s[0xA], 4);
    Inc(s[9]);
    if (Sc()[9] == 0x14) Inc(Sc()[2]);
}

// original 0x4B8530: a 0x30-wide flat-textured quad (tpage 0 / 0x380 / 0x100,
// CLUT 0x20 / 0x1FA, shade 0x80) from the screen point down +0xA, u 0..0x40,
// v 0x20 .. 0x20 + +0xA; after a draw-mode packet, layer 2.
S15_EXPORT void __cdecl InfluenceMark_Draw(void) {
    DrawModeCommit(0x15, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    PutFloat(p + 8, S16(Sc() + 0x2E));
    PutFloat(p + 0xC, S16(Sc() + 0x30));
    PutFloat(p + 0x18, S16(Sc() + 0x2E) + 0x30);
    PutFloat(p + 0x1C, S16(Sc() + 0x30));
    PutFloat(p + 0x28, S16(Sc() + 0x2E));
    PutFloat(p + 0x2C, S16(Sc() + 0x30) + Sc()[0xA]);
    PutFloat(p + 0x38, S16(Sc() + 0x2E) + 0x30);
    PutFloat(p + 0x3C, S16(Sc() + 0x30) + Sc()[0xA]);
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 0, 0x380, 0x100);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0x20, 0x1FA);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
    p[0x14] = 0;
    p[0x15] = 0x20;
    p[0x24] = 0x40;
    p[0x25] = 0x20;
    p[0x34] = 0;
    p[0x35] = static_cast<unsigned char>(Sc()[0xA] + 0x20);
    p[0x44] = 0x40;
    p[0x45] = static_cast<unsigned char>(Sc()[0xA] + 0x20);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x4B8690: four fans of three semi-transparent gouraud triangles
// (layer 3), one at each quarter turn: each fan's centre on the circle of
// radius +0xA about the screen point, its triangles to the points at the
// angles ((Influence_TrianglePoints[j] + 16 k) & 0x3F) << 6 at radius
// (+0xA >> 2) + 1; the shade (0x903856, 0x903858, 1) - 0x80 - 48 x +0xB twice
// before step 2, else 0x80 + 6 x +9 and 0x80 - 6 x +9.
S15_EXPORT void __cdecl Influence_DrawTriangles(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xA]);
        SetSW(2, static_cast<unsigned>(s[0xA] >> 2) + 1);
        if (s[2] < 2) {
            SetSW(6, FadeShade(s));
            SetSW(8, FadeShade(s));
        } else {
            SetSW(6, static_cast<unsigned>(s[9]) * 6 + 0x80);
            SetSW(8, 0x80u - static_cast<unsigned>(s[9]) * 6);
        }
    }
    const unsigned char* const points = Mem(::bof3::Img(Influence_TrianglePoints_addr));
    for (int k = 0, angle = 0; angle < 0x1000; ++k, angle += 0x400) {
        int v = MH_CALL(Math_Sin)(angle);
        SetSW(0xC, static_cast<unsigned>(Mul12(v, SS(0))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
        v = MH_CALL(Math_Cos)(angle);
        SetSW(0xE, static_cast<unsigned>(Mul12(v, SS(0))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        const unsigned turn = static_cast<unsigned>(k) << 4;
        for (unsigned j = 0; j < 3; ++j) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG3)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            PutFloat(p + 8, SS(0xC));
            PutFloat(p + 0xC, SS(0xE));
            {
                const auto a = static_cast<short>(((points[j] + turn) & 0x3Fu) << 6);
                SetSW(4, static_cast<unsigned short>(a));
                v = MH_CALL(Math_Sin)(a);
                PutFloat(p + 0x18, Mul12(v, SS(2)) + SS(0xC));
                v = MH_CALL(Math_Cos)(SS(4));
                PutFloat(p + 0x1C, Mul12(v, SS(2)) + SS(0xE));
            }
            {
                const auto a = static_cast<short>(((points[j + 1] + turn) & 0x3Fu) << 6);
                SetSW(4, static_cast<unsigned short>(a));
                v = MH_CALL(Math_Sin)(a);
                PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(0xC));
                v = MH_CALL(Math_Cos)(SS(4));
                PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xE));
            }
            for (unsigned c : {4u, 0x14u, 0x24u}) {
                p[c] = SB(6);
                p[c + 1] = SB(8);
                p[c + 2] = 1;
            }
            MH_CALL(Gfx_CommitPrim)(3, 0x34);
        }
    }
}

// original 0x4B8910: a dashed ring - 32 of 64 segments (those whose index has
// bit 2 or bit 3 set but not both) of radius +0xA about the screen point, each
// a semi-transparent flat line (layer 3) shaded 0x80 - 48 x +0xB before step
// 2, else 0x80.
S15_EXPORT void __cdecl Influence_DrawRing(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xA]);
        SetSW(0xC, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(0xE, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        SetSW(6, s[2] < 2 ? FadeShade(s) : 0x80u);
    }
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned quarter = i & 0xC;
        if (quarter != 4 && quarter != 8) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const auto a = static_cast<short>(i << 6);
            SetSW(4, static_cast<unsigned short>(a));
            int v = MH_CALL(Math_Sin)(a);
            PutFloat(p + 8, Mul12(v, SS(0)) + SS(0xC));
            v = MH_CALL(Math_Cos)(SS(4));
            PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xE));
        }
        {
            const auto a = static_cast<short>((i + 1) << 6);
            SetSW(4, static_cast<unsigned short>(a));
            int v = MH_CALL(Math_Sin)(a);
            PutFloat(p + 0x14, Mul12(v, SS(0)) + SS(0xC));
            v = MH_CALL(Math_Cos)(SS(4));
            PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xE));
        }
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        MH_CALL(Gfx_CommitPrim)(3, 0x20);
    }
}

void InfluenceRight_Inject() {
    BOF3_INJECT(Influence_Task);
    BOF3_INJECT(Influence_Start);
    BOF3_INJECT(InfluenceChild_Task);
    BOF3_INJECT(InfluenceRight_Run);
    BOF3_INJECT(InfluenceRight_Launch);
    BOF3_INJECT(InfluenceRight_Slide);
    BOF3_INJECT(InfluenceRight_Pick);
    BOF3_INJECT(InfluenceRight_Blink);
    BOF3_INJECT(InfluenceRight_Shrink);
    BOF3_INJECT(InfluenceLeft_Run);
    BOF3_INJECT(InfluenceLeft_Launch);
    BOF3_INJECT(InfluenceLeft_Slide);
    BOF3_INJECT(InfluenceMark_Run);
    BOF3_INJECT(InfluenceMark_Start);
    BOF3_INJECT(InfluenceMark_Brighten);
    BOF3_INJECT(InfluenceMark_Fade);
    BOF3_INJECT(InfluenceMark_Show);
    BOF3_INJECT(InfluenceMark_Draw);
    BOF3_INJECT(Influence_DrawTriangles);
    BOF3_INJECT(Influence_DrawRing);
}

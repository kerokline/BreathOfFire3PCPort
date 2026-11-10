// Moved here by tools/move_funcs.py from magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/sacrifice_actor.h"

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

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return move_script::Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3 (a party index lands below the
// enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A task's copy of a record's first 0x80 bytes (`rep movsd`: dword by dword,
// forward).
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
}

// This group's functions, and other groups' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using AnimFn = void (__cdecl*)(std::uint32_t, std::uint32_t);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

constexpr ::bof3::image::CodeVa kEnemyAnim{bof3::addr::BattleEnemy_SetAnimationOf};   // 0x435A70

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S10_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S10_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// All four projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S10_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The acting actor's record by the byte 0x904B34 (read once): a party member
// at 2 or less, else the enemy by index - 3, unchecked.
unsigned char* ActingRecord(unsigned actor) { return actor < 3 ? PartyRecord(actor) : EnemyRecord(actor); }

// The copy tinted (+0 | 0x20, +0x5C 1, the tint +0x5D..+0x5F 0) and +2 on.
void TintOn(unsigned char* s) {
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x5C] = 1;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    Inc(s[2]);
}

void TintBy(unsigned char* s, unsigned by) {
    AddB(s[0x5D], by);
    AddB(s[0x5E], by);
    AddB(s[0x5F], by);
}

// Animation `base` + the direction on the acting member's record (made
// Sprite_Current for the call) and then on this task's.
void MemberAndCopyAnimation(unsigned actor, unsigned base) {
    unsigned char* const self = Sprite_Current;
    unsigned char* const rec = PartyRecord(actor);
    Sprite_Current = rec;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rec[8] + base));
    Sprite_Current = self;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(self[8] + base));
}

}  // namespace

// original 0x4AD8A0: the kind-2 task. A three-entry stack table by +1:
// Sacrifice_Start, Sacrifice_Wait, MagicFx_DoneAndFree.
S10_EXPORT void __cdecl Sacrifice_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Sacrifice_Start, addr::Sacrifice_Wait, addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "Sacrifice_Task");
}

// original 0x4AD8D0: the task at the side's centre; +0xB and +9 0, +1 on; the
// actor's animation 8; a child (kind 1, 0x55, +1 3: the actor) holding a copy
// of the acting actor's record; CLUT row 26's words 1..15 and 17..31 back
// with their STP bits, words 0 and 16 cleared; the owner's CLUT copied to the
// effect row (its answer to +0x27), +0x28 / +0x24 the owner's, the STP bits
// of this task's CLUT; the owner's +0 | 0x40.
S10_EXPORT void __cdecl Sacrifice_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(8, 0);
    const unsigned slot = NewTask(0x55);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    CopyRecord(child, ActingRecord(ActorByte()));
    unsigned char* const self = Sc();
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 3;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x55;
    Inc(self[0xB]);
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) {
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
        Gfx_ClutStrip[k + 0x10] = static_cast<unsigned short>(Gfx_ClutStripSource[k + 0x10] | 0x8000);
    }
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStrip[0x1A10] = 0;
    Gfx_ClutStripDirty = 1;
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
}

// original 0x4ADA30: once the children are gone (+0xB 0): the owner's 0x40
// bit cleared; the effect row back; the actor's animation 0x1C; the target
// flagged 0x40; the acting actor's record's state +1 6, +2 4, two words
// (+0x98 / +0x90 a member's, +0xA4 / +0x92 an enemy's) 0 and 0x4000, its tint
// released; +1 on.
S10_EXPORT void __cdecl Sacrifice_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    MH_CALL(SpriteClut_RestoreFxRow)();
    MH_CALL(BattleActor_SetAnimation)(0x1C, 0);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    const unsigned actor = ActorByte();
    unsigned char* const rec = ActingRecord(actor);
    rec[1] = 6;
    rec[2] = 4;
    if (actor < 3) {
        SetWord(rec + 0x98, 0);
        SetWord(rec + 0x90, 0x4000);
    } else {
        SetWord(rec + 0xA4, 0);
        SetWord(rec + 0x92, 0x4000);
    }
    MH_CALL(Sprite_ReleaseTint)(rec);
    Inc(Sc()[1]);
}

// original 0x4ADAF0: the children's kind-1 task, a jmp through
// SacrificeChild_Kinds (four: the ring twice, the disc, the actor) by +1,
// unchecked.
S10_EXPORT void __cdecl SacrificeChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {addr::SacrificeRing_Run, addr::SacrificeRing_Run, addr::SacrificeDisc_Run,
                                                addr::SacrificeActor_Run};
    Dispatch(kKinds, 4, Sc()[1], "SacrificeChild_Task");
}

// original 0x4ADB10: a call through SacrificeRing_Steps (three) by +2; then
// while +0 and +2 are set the actor matrix, the ring, the matrix popped.
S10_EXPORT void __cdecl SacrificeRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::SacrificeFx_TakeOwnerPos, addr::SacrificeRing_Grow,
                                                addr::SacrificeRing_Fade};
    Dispatch(kSteps, 3, Sc()[2], "SacrificeRing_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::SacrificeRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4ADB50: the owner's position; +9 and +0xA 0; +2 on.
S10_EXPORT void __cdecl SacrificeFx_TakeOwnerPos(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4ADBA0: at +9 = 4 the target flagged 0x10; the radius word
// 0x903850 +9 x 28; +9 up; at 0xC +2 on.
S10_EXPORT void __cdecl SacrificeRing_Grow(void) {
    unsigned char* s = Sc();
    if (s[9] == 4) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        s = Sc();
    }
    SetSW(0, s[9] * 28u);
    Inc(s[9]);
    if (s[9] == 0xC) Inc(s[2]);
}

// original 0x4ADBF0: the radius +9 x 4 + 0x120; +0xA and +9 up; at +9 0x28
// the owner's count +0xB down and the task freed.
S10_EXPORT void __cdecl SacrificeRing_Fade(void) {
    unsigned char* const s = Sc();
    SetSW(0, s[9] * 4u + 0x120);
    Inc(s[0xA]);
    Inc(s[9]);
    if (s[9] != 0x28) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ADC40: eight bands of 32 semi-transparent textured gouraud
// quads, band n (1..8) between the circles at the angles A - 0x80 and A (A
// from 0xC00 down by 0x80) of a sphere of radius the word 0x903850; the
// shades 0x81, fading by 8 a band once +9 has passed 0x10 + n; page (0x340,
// 0x100) abr 1, clut (0, 0x1FA), columns k x 8, rows (m + 0x14) x 8 /
// (m + 0x15) x 8 (m 0x18 down); each quad sorted at the task's position
// moved by its outer vertex << 9; tpage 0x35.
S10_EXPORT void __cdecl SacrificeRing_Draw(void) {
    int m = 0x18, n = 1, band = 0xC00;
    do {
        {
            const unsigned char* const s = Sc();
            SetSW(6, 0x81);
            SetSW(8, 0x81);
            const int b = s[9];
            if (n - 1 < b - 0x10) {
                const auto d = static_cast<short>(b - n - 0xF);
                if (d < 0x11) SetSW(6, 0x81u - Shl(d, 3));
                else SetSW(6, 1);
            }
            if (n < b - 0x10) {
                const auto d = static_cast<short>(b - n - 0x10);
                SetSW(8, static_cast<unsigned>(d));
                if (d < 0x11) SetSW(8, 0x81u - Shl(SS(8), 3));
                else SetSW(8, 1);
            }
        }
        const int inner = band - 0x80;
        int v = Cos(inner);
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(band);
        SetSW(4, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Sin(inner);
        {
            const auto h = static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(4, h);
            SetVW(0xC, h);
        }
        v = Sin(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Cos(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Sin(band);
        {
            const auto h = static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(0x14, h);
            SetVW(0x1C, h);
        }
        const auto row0 = static_cast<unsigned char>((m + 0x14) << 3);
        const auto row1 = static_cast<unsigned char>((m + 0x15) << 3);
        unsigned k = 1;
        for (int a = 0x80; a < 0x1080; a += 0x80, ++k) {
            SetVW(0, VW(8));
            SetVW(2, VW(0xA));
            v = Sin(a);
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = Cos(a);
            SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
            SetVW(0x10, VW(0x18));
            SetVW(0x12, VW(0x1A));
            v = Sin(a);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = Cos(a);
            const auto y = static_cast<short>(Mul12(v, SS(4)));
            const std::uint32_t dx = Shl(VS(0x18), 9);
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            SetVW(0x1A, static_cast<unsigned>(y));
            const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->x) + dx);
            const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->z) + Shl(y, 9));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Rtp4(p, 0x14);
            MH_CALL(Gte_PrimDepths4_14)(p);
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            const auto u0 = static_cast<unsigned char>(k << 3);
            const auto u1 = static_cast<unsigned char>((k + 1) << 3);
            p[0x15] = row0;
            p[0x14] = u0;
            p[0x3C] = u0;
            p[0x28] = u1;
            p[0x29] = row0;
            p[0x3D] = row1;
            p[0x50] = u1;
            p[0x51] = row1;
            for (unsigned c : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[c] = SB(8);
            for (unsigned c : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[c] = SB(6);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x54);
        }
        --m;
        band -= 0x80;
        ++n;
    } while (band > 0x800);
}

// original 0x4AE060: a call through SacrificeDisc_Steps (four) by +2; then
// while +0 and +2 are set the actor matrix, the disc, the matrix popped.
S10_EXPORT void __cdecl SacrificeDisc_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::SacrificeFx_TakeOwnerPos, addr::SacrificeDisc_Grow,
                                                addr::SacrificeDisc_Hold, addr::SacrificeDisc_Fade};
    Dispatch(kSteps, 4, Sc()[2], "SacrificeDisc_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::SacrificeDisc_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AE0A0: +0xA up by 2, +9 up; at 0xC +2 on.
S10_EXPORT void __cdecl SacrificeDisc_Grow(void) {
    AddB(Sc()[0xA], 2);
    Inc(Sc()[9]);
    if (Sc()[9] == 0xC) Inc(Sc()[2]);
}

// original 0x4AE0D0: +9 up; at 0x1E +2 on.
S10_EXPORT void __cdecl SacrificeDisc_Hold(void) {
    Inc(Sc()[9]);
    if (Sc()[9] == 0x1E) Inc(Sc()[2]);
}

// original 0x4AE0F0: +9 up unless 0x2E; +0xA down; at 0 the owner's count
// +0xB down and the task freed.
S10_EXPORT void __cdecl SacrificeDisc_Fade(void) {
    unsigned char* const s = Sc();
    if (s[9] != 0x2E) Inc(s[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AE130: 16 semi-transparent gouraud triangles round the origin,
// radius Math_Cos(0x800) x (+9 + 0x84) x 4 past step 1, else x +9 x 48 (sar
// 12); the centre (+0xA x 6, +0xA x 8, +0xA x 8), the rim black; tpage 0x55,
// layer 5.
S10_EXPORT void __cdecl SacrificeDisc_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    if (Sc()[2] > 1) {
        const int v = Cos(0x800);
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(static_cast<int>(Mul(v, s[9] + 0x84) << 2) >> 12));
    } else {
        const int v = Cos(0x800);
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(static_cast<int>(Mul(v, s[9]) * 3u << 4) >> 12));
    }
    {
        const unsigned char* const s = Sc();
        SetSW(6, static_cast<unsigned>(s[0xA]) << 3);
        SetSW(8, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        p[4] = SB(8);
        p[5] = SB(6);
        p[6] = SB(6);
        for (unsigned c : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[c] = 0;
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// original 0x4AE320: the actor copy's steps, a nine-entry stack table by +2
// (_Start, _Play, _Darken, _Lighten, _Split, _Wait, _Return, _End,
// BattleFx_FreeTask); the screen update while +0 and +2 are set.
S10_EXPORT void __cdecl SacrificeActor_Run(void) {
    static constexpr std::uint32_t kSteps[9] = {
        addr::SacrificeActor_Start, addr::SacrificeActor_Play,  addr::SacrificeActor_Darken,
        addr::SacrificeActor_Lighten, addr::SacrificeActor_Split, addr::SacrificeActor_Wait,
        addr::SacrificeActor_Return, addr::SacrificeActor_End,  addr::BattleFx_FreeTask};
    Dispatch(kSteps, 9, Sc()[2], "SacrificeActor_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4AE3A0: +0x27 (the CLUT) the owner's; +2 on.
S10_EXPORT void __cdecl SacrificeActor_Start(void) {
    Sc()[0x27] = Owner()[0x27];
    Inc(Sc()[2]);
}

// original 0x4AE3C0: the script ticked once; at its end the tint on and +2
// on.
S10_EXPORT void __cdecl SacrificeActor_Play(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    TintOn(Sc());
}

// original 0x4AE410: the tint down by 0x10; at +0x5D 0x80 the owner's
// position and +2 on.
S10_EXPORT void __cdecl SacrificeActor_Darken(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    TintBy(s, 0xF0);
    if (s[0x5D] != 0x80) return;
    ts->x = rv::As<records::SpriteHead>(Owner())->x;
    ts->z = rv::As<records::SpriteHead>(Owner())->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(Owner())->height);
    Inc(s[2]);
}

// original 0x4AE480: the tint up by 0x10; at +0x5D 0 the tint off, +9 8, +2
// on.
S10_EXPORT void __cdecl SacrificeActor_Lighten(void) {
    unsigned char* const s = Sc();
    TintBy(s, 0x10);
    if (s[0x5D] != 0) return;
    s[0] = static_cast<unsigned char>(s[0] & 0xDF);
    s[0x5C] = 0;
    s[9] = 8;
    Inc(s[2]);
}

// original 0x4AE4E0: +9 down; at 0 a ring (+1 0) and the disc (+1 2),
// children (kind 1, 0x55) of the owner (0x93B940, not this task) and counted
// in its +0xB; sound 0x100; a member actor's cue 0 (with Field_State its
// record) and animation 0x10 + the direction on the member and on this copy,
// an enemy's animation 4 through the engine's 0x435A70; +9 0x1E, +2 on.
S10_EXPORT void __cdecl SacrificeActor_Split(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    for (unsigned char kind : {0, 2}) {
        const unsigned slot = NewTask(0x55);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const owner = Owner();
        tchild->owner = static_cast<std::int32_t>(Key(owner));
        child[1] = kind;
        Inc(owner[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    std::uint32_t actor = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kActor))));
    if ((actor & 0xFF) < 3) {
        unsigned char* const kept = Field_State;
        Field_State = PartyRecord(actor & 0xFF);
        MH_CALL(Battle_PlayActorCue)(0);
        actor = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kActor))));
        Field_State = kept;
        if ((actor & 0xFF) < 3) {
            MemberAndCopyAnimation(actor & 0xFF, 0x10);
            Sc()[9] = 0x1E;
            Inc(Sc()[2]);
            return;
        }
    }
    MH_AT(AnimFn, kEnemyAnim.v)(actor, 4);
    Sc()[9] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4AE620: the script ticked (its answer unread); +9 down; at 0
// the tint on and +2 on.
S10_EXPORT void __cdecl SacrificeActor_Wait(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) TintOn(s);
}

// original 0x4AE680: the script ticked; the tint down by 0x10; at +0x5D
// 0x80 the position of the acting actor's sprite (0x904B3C), a member's
// animation 0x1C + the direction on it and on this copy, an enemy's 4
// through 0x435A70; +2 on.
S10_EXPORT void __cdecl SacrificeActor_Return(void) {
    MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        TintBy(s, 0xF0);
        if (s[0x5D] != 0x80) return;
        const unsigned char* const actor_sprite = Pointer(::bof3::Img(MagicS23_ActorSprite_addr));
        ts->x = rv::As<records::SpriteHead>(actor_sprite)->x;
        ts->z = rv::As<records::SpriteHead>(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)))->z;
        rv::Store(ts->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)))->height);
    }
    const std::uint32_t actor = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kActor))));
    if ((actor & 0xFF) < 3) MemberAndCopyAnimation(actor & 0xFF, 0x1C);
    else MH_AT(AnimFn, kEnemyAnim.v)(actor, 4);
    Inc(Sc()[2]);
}

// original 0x4AE760: while the owner's count +0xB is 1 the tint up by 0x10;
// at +0x5D 0 the owner's count down and +2 on.
S10_EXPORT void __cdecl SacrificeActor_End(void) {
    if (Owner()[0xB] != 1) return;
    unsigned char* const s = Sc();
    TintBy(s, 0x10);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

void SacrificeActor_Inject() {
    BOF3_INJECT(Sacrifice_Task);
    BOF3_INJECT(Sacrifice_Start);
    BOF3_INJECT(Sacrifice_Wait);
    BOF3_INJECT(SacrificeChild_Task);
    BOF3_INJECT(SacrificeRing_Run);
    BOF3_INJECT(SacrificeFx_TakeOwnerPos);
    BOF3_INJECT(SacrificeRing_Grow);
    BOF3_INJECT(SacrificeRing_Fade);
    BOF3_INJECT(SacrificeRing_Draw);
    BOF3_INJECT(SacrificeDisc_Run);
    BOF3_INJECT(SacrificeDisc_Grow);
    BOF3_INJECT(SacrificeDisc_Hold);
    BOF3_INJECT(SacrificeDisc_Fade);
    BOF3_INJECT(SacrificeDisc_Draw);
    BOF3_INJECT(SacrificeActor_Run);
    BOF3_INJECT(SacrificeActor_Start);
    BOF3_INJECT(SacrificeActor_Play);
    BOF3_INJECT(SacrificeActor_Darken);
    BOF3_INJECT(SacrificeActor_Lighten);
    BOF3_INJECT(SacrificeActor_Split);
    BOF3_INJECT(SacrificeActor_Wait);
    BOF3_INJECT(SacrificeActor_Return);
    BOF3_INJECT(SacrificeActor_End);
}

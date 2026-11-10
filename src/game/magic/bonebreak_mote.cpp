// Moved here by tools/move_funcs.py from magic_s07.cpp (docs/source-layout-round.md).
#include "game/magic/bonebreak_mote.h"

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

using move_script::Long;

using move_script::SetLong;

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

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

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

unsigned char* PoolRecord(unsigned index) { return Mem(::bof3::Img(BonebreakMote_Pool_addr) + index * 0x84u); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

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

}  // namespace

#define S07_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A3B20: the kind-2 task. A three-entry stack table by +1 -
// Bonebreak_Start, MAGIC017's 0x4A13B0, MagicFx_DoneAndFree - then the mote
// pool walked: every record with bit 0 becomes Sprite_Current, its +0x80 the
// owner, for BonebreakMote_Task; both put back after each (as read after the
// phase call).
S07_EXPORT void __cdecl Bonebreak_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Bonebreak_Start, bof3::addr::Magic017_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "Bonebreak_Task");
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(bof3::addr::BonebreakMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// original 0x4A3BA0: the pool's records +0..+2 cleared; +0xB 0, +1 on; the
// actor animated (offset 0xC, 2); a child (kind 1, 9) made a copy of the
// acting actor's record's first 0x80 bytes (a party record at 0..2, else the
// enemy's; the actor read after the create), then +0x80 this task, +1 / +2 0,
// +6 1, +5 9; +0xB up; the owner's byte +0 given bit 6.
S07_EXPORT void __cdecl Bonebreak_Start(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    const unsigned slot = NewTask(9);
    const unsigned actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const src = actor <= 2 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(src + k));
    unsigned char* const self = Sc();
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 9;
    Inc(self[0xB]);
    Owner()[0] |= 0x40;
}

// original 0x4A3C80: the copy's kind-1 task, a jmp through
// BonebreakChild_TaskTable (one entry) by +1, unchecked.
S07_EXPORT void __cdecl BonebreakChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::BonebreakChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "BonebreakChild_Task");
}

// original 0x4A3CA0: a six-entry stack table by +2 - BattleFx_SetSize, _Burst,
// _WaitScript, _Shade, _WaitMotes, BattleFx_FreeTask - then, with byte +0 not
// 0, Sprite_UpdateScreen.
S07_EXPORT void __cdecl BonebreakChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::BattleFx_SetSize,        bof3::addr::BonebreakChild_Burst,
                                                bof3::addr::BonebreakChild_WaitScript, bof3::addr::BonebreakChild_Shade,
                                                bof3::addr::BonebreakChild_WaitMotes,  bof3::addr::BattleFx_FreeTask};
    Dispatch(kSteps, 6, Sc()[2], "BonebreakChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A3D00: the script ticked once; +9 down; at 0 sound 0x100, +9
// and +0xB 0, and twelve motes from the pool (BonebreakMote_Alloc, the index
// untested): +0x80 this task (read after each alloc), +9 the delay (i / 2) x
// 3 + 1; +0xB up for each; +2 on.
S07_EXPORT void __cdecl BonebreakChild_Burst(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned index = MH_AT(ByteFn, bof3::addr::BonebreakMote_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = PoolRecord(index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Key(self));
        rec[9] = static_cast<unsigned char>((i >> 1) * 3 + 1);
        Inc(self[0xB]);
    }
    Inc(Sc()[2]);
}

// original 0x4A3DA0: the script ticked once; when it reports its end, target
// flag 0x40 and +2 on.
S07_EXPORT void __cdecl BonebreakChild_WaitScript(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sc()[2]);
}

// original 0x4A3DC0: below +9 0xC MAGIC008's screen shade (0x4A29C0) and +9
// up by 4; else +2 on.
S07_EXPORT void __cdecl BonebreakChild_Shade(void) {
    if (Sc()[9] >= 0xC) {
        Inc(Sc()[2]);
        return;
    }
    Call0(bof3::addr::Magic008_DrawFlash);
    AddB(Sc()[9], 4);
}

// original 0x4A3DE0: once +0xB (the motes alive) is 0, the owner's +0xB down
// and +2 on.
S07_EXPORT void __cdecl BonebreakChild_WaitMotes(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A3E10: a pool record's task, a jmp through
// BonebreakMote_TaskTable (one entry) by +1, unchecked.
S07_EXPORT void __cdecl BonebreakMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::BonebreakMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "BonebreakMote_Task");
}

// original 0x4A3E30: a call through BonebreakMote_Steps (three entries:
// _Launch, MagicFx_CountUp9By2, _Fade) by +2; then with bytes +0 and +2 not 0
// the screen point, the fan, the ring (a tail jump in the original).
S07_EXPORT void __cdecl BonebreakMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BonebreakMote_Launch, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::BonebreakMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "BonebreakMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::BonebreakMote_DrawFan);
    Call0(bof3::addr::BonebreakMote_DrawRing);
}

// original 0x4A3E70: +9 down; at 0 the mote starts at the source sprite
// (0x904B4C): +0x34 / +0x38 its, word +0x3E its + (Rand & 7) sl 5; the speed
// word 0x903850 (0xC - that) sl 4; the angle word 0x903854 (Rand & 7) sl 9;
// the position moved by sin / cos x speed sar 3 (the speed and the angle read
// again after each call); +9 0, +0xA 0x10, +2 on.
S07_EXPORT void __cdecl BonebreakMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned k = RandCall() & 7;
    const unsigned char* const src = Source();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, ((k << 5) + rv::As<records::SpriteHead>(src)->height_whole) & 0xFFFF);
    SetSW(0, (0xC - k) << 4);
    const unsigned angle = (RandCall() & 7) << 9;
    unsigned char* const x = Sc() + 0x34;
    SetSW(4, angle);
    int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
    AddLong(x, static_cast<std::uint32_t>(Imul(v, SS(0)) >> 3));
    unsigned char* const z = Sc() + 0x38;
    v = MH_CALL(Math_Cos)(SS(4));
    AddLong(z, static_cast<std::uint32_t>(Imul(v, SS(0)) >> 3));
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A3F60: +0xA down by 2; at 0 the owner's +0xB down and MAGIC219's
// 0x4F6290 (the record's first five bytes cleared: a tail jump).
S07_EXPORT void __cdecl BonebreakMote_Fade(void) {
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4A3F90: a fan of 16 semi-transparent gouraud triangles round the
// screen point (+0x2E / +0x30), radius +9 x 2, the angle 0x100 a step; the
// centre shaded (+0xA x 12, x 11, x 4), the rim (x 12, x 8, x 4), bytes; each
// linked at the task on layer 2 behind a draw mode (tpage 0x35). The scratch
// (radius 0x903850, angle 0x903854, centre 0x903858 / 0x90385A) read again
// after every call.
S07_EXPORT void __cdecl BonebreakMote_DrawFan(void) {
    DrawMode(0x35);
    LinkAtSprite(2, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    const unsigned a = s[0xA];
    const auto c0 = static_cast<unsigned char>(a * 12), c1 = static_cast<unsigned char>(a * 11),
               c2 = static_cast<unsigned char>(a << 2), c3 = static_cast<unsigned char>(a << 3);
    SetSW(0, static_cast<unsigned>(s[9]) << 1);
    SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(8));
        PutFloat(p + 0xC, SS(0xA));
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xA));
        const unsigned next = static_cast<unsigned>(i + 1) << 8;
        SetSW(4, next);
        v = MH_CALL(Math_Sin)(static_cast<short>(next));
        PutFloat(p + 0x28, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x2C, Mul12(v, SS(0)) + SS(0xA));
        p[4] = c0;
        p[5] = c1;
        p[6] = c2;
        p[0x14] = c0;
        p[0x15] = c3;
        p[0x16] = c2;
        p[0x24] = c0;
        p[0x25] = c3;
        p[0x26] = c2;
        LinkAtSprite(2, 0x34);
    }
}

// original 0x4A4160: a ring of 16 semi-transparent gouraud quads round the
// screen point, from radius +9 x 2 (0x903850) to +9 x 3 (0x903852); the
// inner edge shaded (+0xA x 12, x 6, x 4), the outer 1; as the fan otherwise.
S07_EXPORT void __cdecl BonebreakMote_DrawRing(void) {
    DrawMode(0x35);
    LinkAtSprite(2, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    const unsigned a = s[0xA];
    const auto c0 = static_cast<unsigned char>(a * 12), c1 = static_cast<unsigned char>(a * 6),
               c2 = static_cast<unsigned char>(a << 2);
    SetSW(0, static_cast<unsigned>(s[9]) << 1);
    SetSW(2, static_cast<unsigned>(s[9]) * 3);
    SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 8, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xA));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xA));
        const unsigned next = static_cast<unsigned>(i + 1) << 8;
        SetSW(4, next);
        v = MH_CALL(Math_Sin)(static_cast<short>(next));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xA));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x38, Mul12(v, SS(2)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x3C, Mul12(v, SS(2)) + SS(0xA));
        p[4] = c0;
        p[5] = c1;
        p[6] = c2;
        p[0x14] = c0;
        p[0x15] = c1;
        p[0x16] = c2;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        LinkAtSprite(2, 0x44);
    }
}

// original 0x4A43E0: the mote pool's alloc: the first of its 64 records with
// bit 0 clear gets it, its index in al; 0xFF when all are taken.
S07_EXPORT unsigned char __cdecl BonebreakMote_Alloc(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void BonebreakMote_Inject() {
    BOF3_INJECT(Bonebreak_Task);
    BOF3_INJECT(Bonebreak_Start);
    BOF3_INJECT(BonebreakChild_Task);
    BOF3_INJECT(BonebreakChild_Run);
    BOF3_INJECT(BonebreakChild_Burst);
    BOF3_INJECT(BonebreakChild_WaitScript);
    BOF3_INJECT(BonebreakChild_Shade);
    BOF3_INJECT(BonebreakChild_WaitMotes);
    BOF3_INJECT(BonebreakMote_Task);
    BOF3_INJECT(BonebreakMote_Run);
    BOF3_INJECT(BonebreakMote_Launch);
    BOF3_INJECT(BonebreakMote_Fade);
    BOF3_INJECT(BonebreakMote_DrawFan);
    BOF3_INJECT(BonebreakMote_DrawRing);
    BOF3_INJECT(BonebreakMote_Alloc);
}

// Moved here by tools/move_funcs.py from magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/lavaburst_child.h"

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

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle (its kind)
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

// MAGIC053's own pool of 32 task-like records (Lavaburst_Pool) and the byte
// table its start reads (Lavaburst_ChildDelays), both read in place.
// kPool: now Lavaburst_Pool (symbols.toml).
constexpr unsigned kPoolRecords = 32;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return move_script::Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PoolRecord(unsigned index) { return Mem(::bof3::Img(Lavaburst_Pool_addr) + index * 0x84u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions, and other groups' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

using TaskFn = void (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

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

// The pool walked: every record with bit 0 becomes Sprite_Current, its +0x80
// the owner, for LavaburstRecord_Task; both put back after each. The task and
// owner are read after the phase call that precedes the walk.
void WalkPool() {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(addr::LavaburstRecord_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The radius word 0x903850 x the sine or cosine of `angle`, sar 12.
short Scaled(int v) { return static_cast<short>(Mul12(v, SS(0))); }

// The record at the angle +0xB << 9 round the owner, at the radius +0xC (no
// shift): the angle word 0x903854 read again for the cosine.
void RecordOrbit() {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 9);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(Mul(v, ts->step_x) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x));
    }
    v = Cos(SS(4));
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->z = static_cast<std::int32_t>(Mul(v, ts->step_x) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z));
}

}  // namespace

// original 0x4ABF20: the kind-2 task. A three-entry stack table by +1
// (Lavaburst_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish), then the pool
// walked.
S10_EXPORT void __cdecl Lavaburst_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Lavaburst_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Lavaburst_Task");
    WalkPool();
}

// original 0x4ABFA0: the pool's +0..+2 cleared; the task at the side's
// centre and on the screen; its direction the acting actor's sprite's,
// turned round (xor 2) when the target's side bit names the other side from
// the actor (0x40 with a party actor, none with an enemy);
// +0xB 0, +9 0x10, +1 on; eight children (kind 1, 0x5E), +4 / +9 from
// Lavaburst_ChildDelays; CLUT row 26 and the first 16 words of row 2 back
// with their STP bits, the first word of each without.
S10_EXPORT void __cdecl Lavaburst_Start(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned actor = ActorByte();
    unsigned char facing = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
    if (enemies ? actor < 3 : actor >= 3) facing = static_cast<unsigned char>(facing ^ 2);
    Sc()[8] = facing;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x5E);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        const unsigned char delay = Mem(::bof3::Img(Lavaburst_ChildDelays_addr))[i];
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(delay >> 4);
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(delay + 1);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
}

// original 0x4AC0E0: the child's kind-1 task, a jmp through
// LavaburstChild_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl LavaburstChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::LavaburstChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "LavaburstChild_Task");
}

// original 0x4AC100: with the frame-offset table the effects' (0x8E3580), a
// call through LavaburstChild_Steps (five) by +2; then while bit 0 of +0 is
// set and +2 is 1..3: the screen point and update, MAGIC219's 0x4F6020, the
// glow, MAGIC226/227's 0x4FA440, the matrix popped; the battle's table back.
S10_EXPORT void __cdecl LavaburstChild_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::LavaburstChild_Launch, addr::LavaburstChild_Rise,
                                                addr::LavaburstChild_Shake, addr::LavaburstChild_Settle,
                                                addr::LavaburstChild_End};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 5, Sc()[2], "LavaburstChild_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0 && s[2] < 4) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::LavaburstChild_DrawGlow);
        Call0(addr::MeteorStrikeRock_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4AC160: +9 down; at 0 the child starts: at an angle +0xB << 9
// round the owner, radius 0x18 or 0x28 (Rand & 0x10); the owner's direction;
// lifted by (0x20000, 0) turned plus a height 0x6000000 over the owner's;
// the step (-0x2000, 0) turned, the fall 0x600000; its sprite fields,
// animation 0; +0xB 0, +9 and +0xA 0x10, +2 on.
S10_EXPORT void __cdecl LavaburstChild_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetSW(4, static_cast<unsigned>(Sc()[0xB]) << 9);
    const std::uint32_t r = RandCall();
    const int angle = SS(4);
    SetSW(0, (r & 0x10) + 0x18);
    int v = Sin(angle);
    {
        const std::uint32_t x = Mul(v, SS(0)) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x);
        rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(x));
    }
    v = Cos(SS(4));
    {
        const std::uint32_t z = Mul(v, SS(0)) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z);
        rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(z));
    }
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x6000000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
        AddLong(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
        rv::Store(ts->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) +
                                                    static_cast<std::uint32_t>(ts->step_y)));
        ts->step_x = static_cast<std::int32_t>(0xFFFFE000u);
        ts->step_z = 0;
        rv::Store(ts->work_20, 0x600000);
    }
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        rv::Store(rv::As<records::BattleTask>(s)->scale_x, 0x10000);
        rv::Store(rv::As<records::BattleTask>(s)->scale_y, 0x10000);
        s[0x48] = 2;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
        s[0x2B] = 1;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4AC360: the child rises: +0x34 / +0x38 on by the step, the
// height +0x14 down by the fall +0x20 and set over the owner's, +0xB up by 8,
// +9 down; at 0 a sound (0x101 or 0x100 by bit 0 of +4), eight pool records
// (Lavaburst_PoolAlloc; a full pool skips one) owned by this task and
// numbered, counted in +4; animation 1; the camera's first angle up by 0x14;
// +9 4, +2 on.
S10_EXPORT void __cdecl LavaburstChild_Rise(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
        AddLong(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
        AddLong(s + 0x14, 0u - static_cast<std::uint32_t>(ts->work_20));
        rv::Store(ts->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) +
                                                    static_cast<std::uint32_t>(ts->step_y)));
        AddB(s[0xB], 8);
        Dec(s[9]);
        if (s[9] != 0) return;
    }
    MH_CALL(Sound_PlayById)(static_cast<unsigned short>((Sc()[4] & 1) ? 0x101 : 0x100));
    Sc()[4] = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned index = MH_AT(ByteFn, addr::Lavaburst_PoolAlloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = PoolRecord(index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        unsigned char* const self = Sc();
        trec->owner = static_cast<std::int32_t>(Key(self));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        Inc(self[4]);
    }
    MH_CALL(Sprite_SetAnimation)(1);
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4AC470: the script ticked once (its answer unread); the camera's
// first angle up or down by 0x14 by bit 0 of +9, +9 down; at 0 the angle
// 0xFD56 and +2 on.
S10_EXPORT void __cdecl LavaburstChild_Shake(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) ? 0x14 : -0x14));
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Inc(s[2]);
}

// original 0x4AC4C0: +0xA down by 2 unless 0; the script ticked once, and at
// its end +2 on.
S10_EXPORT void __cdecl LavaburstChild_Settle(void) {
    unsigned char* const s = Sc();
    if (s[0xA] != 0) s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) != 0) Inc(Sc()[2]);
}

// original 0x4AC4F0: once its records are gone (+4 0), the owner's count
// +0xB down and the task freed.
S10_EXPORT void __cdecl LavaburstChild_End(void) {
    if (Sc()[4] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AC510: a glow of 64 semi-transparent gouraud triangles round
// the origin, radius +0xB, all nine colour bytes +0xA x 4; tpage 0x55,
// layer 5.
S10_EXPORT void __cdecl LavaburstChild_DrawGlow(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) << 2);
    }
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Scaled(v)));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Scaled(v)));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Scaled(v)));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Scaled(v)));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// original 0x4AC6B0: a pool record's task, a jmp through
// LavaburstRecord_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl LavaburstRecord_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::LavaburstRecord_Run};
    Dispatch(kKinds, 1, Sc()[1], "LavaburstRecord_Task");
}

// original 0x4AC6D0: a call through LavaburstRecord_Steps (three: MAGIC222's
// 0x4F7C40, _Grow, _Shrink) by +2; then while +0 and +2 are set the actor
// matrix, the quad, the matrix popped.
S10_EXPORT void __cdecl LavaburstRecord_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CombustionMote_Start, addr::LavaburstRecord_Grow,
                                                addr::LavaburstRecord_Shrink};
    Dispatch(kSteps, 3, Sc()[2], "LavaburstRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::LavaburstRecord_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AC710: the radius +0xC up by 2, the record on its orbit; +9
// down, at 0 +2 on.
S10_EXPORT void __cdecl LavaburstRecord_Grow(void) {
    AddLong(Sc() + 0xC, 2);
    RecordOrbit();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4AC7A0: the radius up by 1, the orbit; +0xA down, at 0 the
// owner's record count +4 down and MAGIC219's pool free (a tail jmp).
S10_EXPORT void __cdecl LavaburstRecord_Shrink(void) {
    AddLong(Sc() + 0xC, 1);
    RecordOrbit();
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[4]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4AC840: one semi-transparent textured quad of radius 0x100 at
// the angles 0x200, 0x600, 0xE00, 0xA00; page (0x340, 0x100) abr 1, clut
// (0, 0x1E2), the texture's 0x20 x 0x20 at (0, 0x80); the colour +0xA x 6;
// tpage 0x55, layer 5.
S10_EXPORT void __cdecl LavaburstRecord_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetSW(0, 0x100);
    SetSW(0xA, Sc()[0xA] * 6u);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        SetVW(c * 8, static_cast<unsigned>(Scaled(v)));
        v = Sin(kAngles[c]);
        SetVW(c * 8 + 4, 0);
        SetVW(c * 8 + 2, static_cast<unsigned>(Scaled(v)));
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x14] = 0;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = SB(0xA);
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    Rtp4(p, 0x10);
    MH_CALL(Gte_PrimDepths4_10)(p);
    MH_CALL(Gfx_CommitPrim)(5, 0x48);
}

// original 0x4ACA50: the first of the pool's 32 records without bit 0 of +0
// gets it; its index in al, 0xFF when all are taken.
S10_EXPORT unsigned char __cdecl Lavaburst_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void LavaburstChild_Inject() {
    BOF3_INJECT(Lavaburst_Task);
    BOF3_INJECT(Lavaburst_Start);
    BOF3_INJECT(LavaburstChild_Task);
    BOF3_INJECT(LavaburstChild_Run);
    BOF3_INJECT(LavaburstChild_Launch);
    BOF3_INJECT(LavaburstChild_Rise);
    BOF3_INJECT(LavaburstChild_Shake);
    BOF3_INJECT(LavaburstChild_Settle);
    BOF3_INJECT(LavaburstChild_End);
    BOF3_INJECT(LavaburstChild_DrawGlow);
    BOF3_INJECT(LavaburstRecord_Task);
    BOF3_INJECT(LavaburstRecord_Run);
    BOF3_INJECT(LavaburstRecord_Grow);
    BOF3_INJECT(LavaburstRecord_Shrink);
    BOF3_INJECT(LavaburstRecord_Draw);
    BOF3_INJECT(Lavaburst_PoolAlloc);
}

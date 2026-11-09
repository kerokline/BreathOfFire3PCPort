// Moved here by tools/move_funcs.py from magic_s38.cpp (docs/source-layout-round.md).
#include "game/magic/meteor_strike_rock.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s38.h"
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

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch
// (0x903850.., Scratch_Swap at +0xC; words or dwords by function, MAGIC225's
// burst colours at +0x10..+0x1B) and the four SVECTORs of Prim_VertexScratch
// (0x9037A0..0x9037BF). Both are read again after every call, as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

// kRecordPool: now MeteorStrike_RecordPool (symbols.toml). MeteorStrike_RecordPool, 48
constexpr unsigned kRecords = 48;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* p, std::uint32_t v) {
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PoolRecord(std::uint32_t pool, unsigned index) { return Mem(pool + index * 0x84u); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions, and other units' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

using TaskFn = void (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

unsigned Alloc(std::uint32_t address) { return MH_WORD(ByteFn, address)() & 0xFFu; }

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

// A pool walked: every record with bit 0 becomes Sprite_Current, its +0x80
// the owner, for the record's task; both put back after each. The task and
// owner are read after the phase call that precedes the walk.
void WalkPool(std::uint32_t pool, unsigned records, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The first `records` of a pool without bit 0 of +0 get it; the index, 0xFF
// when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void ClearPool(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// A child task (kind 1): +0x80 this task, +1 0, counted in this task's +0xB.
// This task is read after BattleTask_Create; the slot is written unchecked
// (0xFF, none free, lands past the image, as in the original).
void AdoptChild(unsigned slot) {
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    Inc(self[0xB]);
}

// The four projected points of Prim_VertexScratch at +8, +8 + step, ... of a
// primitive; Gte_RotTransPers4 gets the depth and flag pointers the originals
// pass.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S38_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S38_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to `slot`.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S38_EXPORT extern "C" __attribute__((disable_tail_calls))

// The chip's drift: the position on by `scale` x the sine / cosine of
// ((+0xB - 2) & 0xF) << 8 (0x903854, read back), the height on by the lift,
// the lift on by the pull.
void ChipDrift(int scale) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const auto angle = static_cast<short>(((s[0xB] - 2u) & 0xFu) << 8);
        SetSW(4, static_cast<unsigned>(angle));
        const int v = Sin(angle);
        AddLong(x, Mul(v, scale));
    }
    {
        unsigned char* const s = Sc();
        const short angle = SS(4);
        unsigned char* const z = s + 0x38;
        const int v = Cos(angle);
        AddLong(z, Mul(v, scale));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x3C, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_y));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    }
}

// The trail at the rock's x / z, its height +0x3C down by +0x14 a frame.
void TrailFollow() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    unsigned char* const s = Sc();
    AddLong(s + 0x3C, 0u - static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_y));
}

// original 0x4F9DE0: the kind-2 task. A three-entry stack table by +1
// (MeteorStrike_Start, MagicFx_CountDownFlag10, BattleFx_Finish), then the
// record pool walked.
S38_EXPORT void __cdecl MeteorStrike_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::MeteorStrike_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "MeteorStrike_Task");
    WalkPool(::bof3::Img(MeteorStrike_RecordPool_addr), kRecords, addr::MeteorStrikeRecord_Task);
}

// original 0x4F9E60: the record pool's +0..+2 cleared; the task at the side's
// centre and on the screen; its direction the acting actor's sprite's,
// turned round (xor 2) when the target's side bit names the other side from
// the actor (0x40 with a party actor, none with an enemy); +0xB 0, +9 0x10,
// +1 on; the rock (kind 1, 0x5F); sound 0x100; CLUT row 26 and the first 16
// words of row 2 back with their STP bits, the first word of each without.
S38_EXPORT void __cdecl MeteorStrike_Start(void) {
    ClearPool(::bof3::Img(MeteorStrike_RecordPool_addr), kRecords);
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
    AdoptChild(NewTask(0x5F));
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    {
        const unsigned short first26 = Gfx_ClutStripSource[0x1A00];
        const unsigned short first2 = Gfx_ClutStripSource[0x200];
        Gfx_ClutStrip[0x1A00] = first26;
        Gfx_ClutStrip[0x200] = first2;
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x4F9F70: a phase ten overlays' stack tables hold (MAGIC053, 124,
// 129, 130, 132, 223, 226 and others): +9 down; at 0 the target flagged 0x10
// and +1 on.
S38_EXPORT void __cdecl MagicFx_CountDownFlag10(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4F9FB0: the rock's kind-1 task, a jmp through
// MeteorStrikeRock_TaskTable (one entry) by +1, unchecked.
S38_EXPORT void __cdecl MeteorStrikeRock_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::MeteorStrikeRock_Run};
    Dispatch(kKinds, 1, Sc()[1], "MeteorStrikeRock_Task");
}

// original 0x4F9FD0: with the effects' frame-offset table, a call through
// MeteorStrikeRock_Steps (five entries) by +2; then while bit 0 of +0 and +2
// are set: the screen point and update, MAGIC219's 0x4F6020 (the matrix),
// MAGIC053's LavaburstChild_DrawGlow, the ground ring, the matrix popped; the
// battle's table back.
S38_EXPORT void __cdecl MeteorStrikeRock_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::MeteorStrikeRock_Launch, addr::MeteorStrikeRock_Fall,
                                                addr::MeteorStrikeRock_Shake, addr::MeteorStrikeRock_Hide,
                                                addr::MeteorStrikeRock_End};
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 5, phase, "MeteorStrikeRock_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        MH_CALL(LavaburstChild_DrawGlow)();
        Call0(addr::MeteorStrikeRock_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4FA030: the rock starts at the owner, its direction; lifted by
// (0x20000, 0) turned and a height +0x14 0x9000000 added to the owner's; the step
// (-0x1000, 0) turned, the fall +0x20 0x300000; its sprite fields (size
// 0x10000, frame table 0x1D / 0x1A, layer 4, ...); animation 0; +4 0, +0xB 0,
// +9 0x20, +0xA 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Launch(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x9000000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
    }
    {
        const std::uint32_t height = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height);
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(height + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_y)));
    }
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0xFFFFF000u));
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0x300000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        rv::Store(rv::As<records::BattleTask>(s)->scale_x, 0x10000);
        rv::Store(rv::As<records::BattleTask>(s)->scale_y, 0x10000);
        s[0x48] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
        s[0x2B] = 0;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x20;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4FA1C0: the rock on by its step, its height +0x14 down by the
// fall, set over the owner's; +0xB up by 6; every fourth frame a trail
// (record kind 1, +9 1) owned by the rock and counted in its +4; +9 down; at
// 0 thirty-two chips owned by the kind-2 task (the rock's owner) and counted
// in its +0xB: +0xB i & 7, +9 (i / 8) x 3 + 1, +0x5D and +0xA from
// MeteorStrikeChip_Shades / _Fades by i / 8 (a full pool skips one); the
// camera's first angle up by 0x14; sound 0x101; +9 8, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Fall(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x14, 0u - static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    }
    {
        const std::uint32_t height = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height);
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(height + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_y)));
    }
    AddB(Sc()[0xB], 6);
    if ((Mem(::bof3::Img(Frame_Counter_addr))[0] & 3) == 0) {
        const unsigned index = Alloc(addr::MeteorStrike_RecordAlloc);
        if (index != 0xFF) {
            unsigned char* const self = Sc();
            unsigned char* const rec = PoolRecord(::bof3::Img(MeteorStrike_RecordPool_addr), index);
            records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
            trec->owner = static_cast<std::int32_t>(Key(self));
            rec[1] = 1;
            rec[9] = 1;
            Inc(self[4]);
        }
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned index = Alloc(addr::MeteorStrike_RecordAlloc);
        if (index == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const rec = PoolRecord(::bof3::Img(MeteorStrike_RecordPool_addr), index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Key(owner));
        rec[1] = 0;
        rec[0x5D] = Mem(::bof3::Img(MeteorStrikeChip_Shades_addr))[i >> 3];
        rec[0xB] = static_cast<unsigned char>(i & 7);
        rec[9] = static_cast<unsigned char>((i >> 3) * 3 + 1);
        rec[0xA] = Mem(::bof3::Img(MeteorStrikeChip_Fades_addr))[i >> 3];
        Inc(owner[0xB]);
    }
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4FA340: the camera's first angle up or down by 0x14 by bit 0 of
// +9, +9 down; at 0 the angle 0xFD56, +9 0x3C, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Shake(void) {
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) ? 0x14 : -0x14));
    Dec(s[9]);
    unsigned char* const t = Sc();
    if (t[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    t[9] = 0x3C;
    Inc(Sc()[2]);
}

// original 0x4FA390: +9 down; at 0 +0 |= 0x20 (the sprite hidden), +0x5C 1,
// the tint +0x5D..+0x5F 0, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Hide(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Inc(Sc()[2]);
}

// original 0x4FA3E0: +0xA down by 2 while not 0; the tint +0x5D..+0x5F down
// by 8 each; at +0x5D 0x80 the owner's count +0xB down and the task freed (a
// tail jmp).
S38_EXPORT void __cdecl MeteorStrikeRock_End(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] != 0) s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    }
    AddB(Sc()[0x5D], 0xF8);
    AddB(Sc()[0x5E], 0xF8);
    AddB(Sc()[0x5F], 0xF8);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FA440: a ground ring under the pushed matrix (MAGIC053's
// Lavaburst calls it too): sixty-four semi-transparent gouraud quads between
// a circle of radius +0xB (0x903850) and one of 2 x +0xB (0x903852), 0x40 of
// angle a step, each vertex pair carried round in Prim_VertexScratch (the
// outer z 0); black 1 inside, grey +0xA x 4 (0x90385A) outside; tpage 0x55,
// layer 5.
S38_EXPORT void __cdecl MeteorStrikeRock_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) << 2);
    }
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int t = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(t));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = Sin(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xA);
        p[0x26] = SB(0xA);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xA);
        p[0x36] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
}

// original 0x4FA670: a record's task, a jmp through MeteorStrikeRecord_Kinds
// (two entries: the chip, the trail) by +1, unchecked.
S38_EXPORT void __cdecl MeteorStrikeRecord_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {addr::MeteorStrikeChip_Run, addr::MeteorStrikeTrail_Run};
    Dispatch(kKinds, 2, Sc()[1], "MeteorStrikeRecord_Task");
}

// original 0x4FA690: a call through MeteorStrikeChip_Steps (three entries) by
// +2; then while +0 and +2 are set, the screen point and the chip (a tail
// jmp).
S38_EXPORT void __cdecl MeteorStrikeChip_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::MeteorStrikeChip_Launch, addr::MeteorStrikeChip_Rise,
                                                addr::MeteorStrikeChip_Fall};
    Dispatch(kSteps, 3, Sc()[2], "MeteorStrikeChip_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::MeteorStrikeChip_Draw);
}

// original 0x4FA6C0: +9 down; at 0 the chip starts 24 x the sine / cosine of
// ((+0xB - 2) & 0xF) << 8 (the word 0x903854, read back for the cosine) round
// the owner, at its height; the lift +0x14 MeteorStrikeChip_Lifts[+0xB & 3],
// the pull +0x20 minus a sixteenth of it; +0x5E 0x10, +9 0x20, +2 on.
S38_EXPORT void __cdecl MeteorStrikeChip_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const auto angle = static_cast<short>(((Sc()[0xB] - 2u) & 0xFu) << 8);
    SetSW(4, static_cast<unsigned>(angle));
    int v = Sin(angle);
    {
        const std::uint32_t base = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x);
        rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(base + Mul(v, 24)));
    }
    v = Cos(SS(4));
    {
        const std::uint32_t base = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z);
        rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(base + Mul(v, 24)));
    }
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Long(Mem(::bof3::Img(MeteorStrikeChip_Lifts_addr) + 4u * (s[0xB] & 3u))));
    }
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->work_20, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(DivPow2(rv::As<records::BattleTask>(s)->step_y, 4))));
    }
    Sc()[0x5E] = 0x10;
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4FA790: the drift at 2 x; +9 down; at 0 the lift again from
// MeteorStrikeChip_Lifts, the pull minus an eighth of it, +9 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeChip_Rise(void) {
    ChipDrift(2);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Long(Mem(::bof3::Img(MeteorStrikeChip_Lifts_addr) + 4u * (s[0xB] & 3u))));
    }
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->work_20, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(DivPow2(rv::As<records::BattleTask>(s)->step_y, 3))));
    }
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4FA860: the drift at 1 x; +0x5E down while not 0; +9 down; at 0
// the owner's count +0xB down and MAGIC219's record free (a tail jmp).
S38_EXPORT void __cdecl MeteorStrikeChip_Fall(void) {
    ChipDrift(1);
    {
        unsigned char* const s = Sc();
        if (s[0x5E] != 0) Dec(s[0x5E]);
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4FA910: one semi-transparent textured quad (tpage 0x35) linked at
// the chip's map point, its corners at the angles 0x200, 0x600, 0xE00, 0xA00
// (cosine for x, sine for y), radius +0xA (0x903850) round the screen point;
// page (0x340, 0x100) abr 1, CLUT (0, 0x1E2), the texture's 0x20 x 0x20 at
// (0, 0x80); grey (signed +0x5E) x 6 (0x90385A), red plus the tint +0x5D.
S38_EXPORT void __cdecl MeteorStrikeChip_Draw(void) {
    DrawMode(0x35);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z),
                                    2, 0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * 6));
        SetSW(0, s[0xA]);
    }
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        PutFloat(p + 8 + 0x10 * c, Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = Sin(kAngles[c]);
        PutFloat(p + 0xC + 0x10 * c, Mul12(v, SS(0)) + S16(Sc() + 0x30));
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
    p[4] = static_cast<unsigned char>(Sc()[0x5D] + SB(0xA));
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z),
                                    2, 0x48);
    }
}

// original 0x4FAB70: a call through MeteorStrikeTrail_Steps (three entries) by
// +2; then while +0 and +2 are set, under the task's matrix
// (MagicFx_PushActorMatrix) the trail, the matrix popped (a tail jmp).
S38_EXPORT void __cdecl MeteorStrikeTrail_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::MeteorStrikeTrail_Start, addr::MeteorStrikeTrail_Follow,
                                                addr::MeteorStrikeTrail_Fade};
    Dispatch(kSteps, 3, Sc()[2], "MeteorStrikeTrail_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::MeteorStrikeTrail_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4FABB0: +9 down; at 0 the trail starts at the owner (the rock):
// its direction and position, its height - 0x3000000; the step +0x14
// 0x60000; +9 8, +0xA 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeTrail_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) - 0x3000000u));
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x60000);
    Sc()[9] = 8;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4FAC40: at the rock, its height down by +0x14; +9 up; at 0x18 +2 on.
S38_EXPORT void __cdecl MeteorStrikeTrail_Follow(void) {
    TrailFollow();
    Inc(Sc()[9]);
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4FAC90: +9 up, +0xA down by 2; at the rock, its height down by +0x14; at
// +0xA 0 the owner's trail count +4 down and MAGIC219's record free (a tail
// jmp).
S38_EXPORT void __cdecl MeteorStrikeTrail_Fade(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 0xFE);
    TrailFollow();
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[4]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4FAD00: the trail - thirty-two semi-transparent gouraud quads
// between a circle of radius +9 x 0x20 (0x903852) at z -0x40 and one of +9 x
// 24 (0x903850) at z 0, 0x80 of angle a step, each vertex pair carried round
// in Prim_VertexScratch; black 1 above, (+0xA x 12, x 6, x 6) (0x90385A..E)
// below; each linked at the task's map point plus its outer vertex << 9;
// tpage 0x35.
S38_EXPORT void __cdecl MeteorStrikeTrail_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[9] * 24u);
        SetSW(2, static_cast<unsigned>(s[9]) << 5);
        SetSW(0xA, s[0xA] * 12u);
        SetSW(0xC, s[0xA] * 6u);
        SetSW(0xE, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0xFFC0);
    SetVW(4, 0xFFC0);
    v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        {
            const std::uint16_t x = VW(0), y = VW(2);
            SetVW(8, x);
            SetVW(0xA, y);
        }
        v = Sin(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int t = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x10);
            SetVW(2, static_cast<unsigned>(t));
            const std::uint16_t y = VW(0x12);
            SetVW(0x18, x);
            SetVW(0x1A, y);
        }
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        const auto outer_z = static_cast<short>(Mul12(v, SS(0)));
        const int outer_x = VS(0x10);
        std::uint32_t map_x, map_z;
        {
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            SetVW(0x12, static_cast<unsigned>(outer_z));
            map_x = static_cast<std::uint32_t>(Shl(outer_x, 9)) + static_cast<std::uint32_t>(ts->x);
            map_z = static_cast<std::uint32_t>(Shl(outer_z, 9)) + static_cast<std::uint32_t>(ts->z);
        }
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(map_x, map_z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xE);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xC);
        p[0x36] = SB(0xE);
        MH_CALL(MapView_LinkPrimAt)(map_x, map_z, 0, 0x48);
    }
}

// original 0x4FAF90: the first of the record pool's 48 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S38_EXPORT unsigned char __cdecl MeteorStrike_RecordAlloc(void) { return PoolAlloc(::bof3::Img(MeteorStrike_RecordPool_addr), kRecords); }

void MeteorStrikeRock_Inject() {
    BOF3_INJECT(MeteorStrike_Task);
    BOF3_INJECT(MeteorStrike_Start);
    BOF3_INJECT(MagicFx_CountDownFlag10);
    BOF3_INJECT(MeteorStrikeRock_Task);
    BOF3_INJECT(MeteorStrikeRock_Run);
    BOF3_INJECT(MeteorStrikeRock_Launch);
    BOF3_INJECT(MeteorStrikeRock_Fall);
    BOF3_INJECT(MeteorStrikeRock_Shake);
    BOF3_INJECT(MeteorStrikeRock_Hide);
    BOF3_INJECT(MeteorStrikeRock_End);
    BOF3_INJECT(MeteorStrikeRock_DrawRing);
    BOF3_INJECT(MeteorStrikeRecord_Task);
    BOF3_INJECT(MeteorStrikeChip_Run);
    BOF3_INJECT(MeteorStrikeChip_Launch);
    BOF3_INJECT(MeteorStrikeChip_Rise);
    BOF3_INJECT(MeteorStrikeChip_Fall);
    BOF3_INJECT(MeteorStrikeChip_Draw);
    BOF3_INJECT(MeteorStrikeTrail_Run);
    BOF3_INJECT(MeteorStrikeTrail_Start);
    BOF3_INJECT(MeteorStrikeTrail_Follow);
    BOF3_INJECT(MeteorStrikeTrail_Fade);
    BOF3_INJECT(MeteorStrikeTrail_Draw);
    BOF3_INJECT(MeteorStrike_RecordAlloc);
}

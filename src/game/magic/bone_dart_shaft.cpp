// Moved here by tools/move_funcs.py from magic_s09.cpp (docs/source-layout-round.md).
#include "game/magic/bone_dart_shaft.h"

#include <climits>
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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// The cells the overlays read beyond the harness's names.
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The task's +8 and position (+0x34 / +0x38 / +0x3C) the owner's.
void FacingAndPositionFromOwner() {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// Gfx_ClutStrip 0x1A00..0x1A0F (row 26's first sixteen words) back from its
// source with the semi-transparency bit, then the first word plain; the
// strip marked dirty first.
void RestoreRow26Head() {
    Gfx_ClutStripDirty = 1;
    for (unsigned k = 0; k < 0x10; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
}

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

// The children's run: the effects' frame-offset table in 0x9039D8 round a
// call through `steps` (four entries) by +2; the screen update while +0 and
// +2 are set.
static void BoneDartChildRun(const std::uint32_t* steps, const char* who) {
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(steps, 4, Sc()[2], who);
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// The shadow under the dart (the task slot +0xB, unchecked: 155 and above lie
// past the image, a fault in both): its x / z, the ground's height there, the
// dart's +0x3C when its height is below the ground's; +2 on when the dart's
// +2 is `step`.
static void BoneDartShadowFollow(unsigned step) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = Long(TaskSlot(s[0xB]) + 0x34);
        ts->z = Long(TaskSlot(s[0xB]) + 0x38);
    }
    const long ground = MH_CALL(AreaMap_Elevation)(rv::Current<records::BattleTask>()->x, rv::Current<records::BattleTask>()->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<std::uint32_t>(ground) & 0xFFFF);
    unsigned char* const s = Sc();
    {
        const unsigned char* const dart = TaskSlot(s[0xB]);
        if (S16(dart + 0x3E) < S16(s + 0x3E)) rv::Store(rv::As<records::BattleTask>(s)->height, rv::As<records::BattleTask>(dart)->height);
    }
    if (TaskSlot(s[0xB])[2] == step) Inc(s[2]);
}

// original 0x4A9830: the kind-2 task. A two-entry stack table by +1:
// BoneDart_Start, MAGIC131's MagicFx_EndWhenChildrenDone.
S09_EXPORT void __cdecl BoneDart_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::BoneDart_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 2, Sc()[1], "BoneDart_Task");
}

// original 0x4A9860: the actor and the target on one side (both below 3 or
// both above 2): the target flagged 0x40, the done flag, the task freed.
// Else the owner's direction and position; two children (kind 1, 0x31): the
// dart (+1 0) and its shadow (+1 1, +0xB the dart's slot), each +0x80 this
// task; row 26's first sixteen CLUT words back with their STP bits; +0xB 2
// (two children), +1 on.
S09_EXPORT void __cdecl BoneDart_Start(void) {
    const unsigned char target = TargetByte();
    const unsigned char actor = Mem(::bof3::Img(at::kActor))[0];
    const bool same_side = (target < 3 && actor < 3) || (target > 2 && actor > 2);
    if (same_side) {
        MH_CALL(Battle_SetTargetFlag40)(target);
        Mem(::bof3::Img(at::kFlags))[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    FacingAndPositionFromOwner();
    const unsigned dart = NewTask(0x31);
    {
        unsigned char* const child = TaskSlot(dart);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(Sc()));
        child[1] = 0;
    }
    const unsigned shadow = NewTask(0x31);
    unsigned char* const self = Sc();
    {
        unsigned char* const child = TaskSlot(shadow);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 1;
        child[0xB] = static_cast<unsigned char>(dart);
    }
    RestoreRow26Head();
    self[0xB] = 2;
    Inc(Sc()[1]);
}

// original 0x4A99A0: the children's kind-1 task, a jmp through
// BoneDartChild_Kinds (two entries) by +1, unchecked.
S09_EXPORT void __cdecl BoneDartChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::BoneDartShaft_Run, bof3::addr::BoneDartShadow_Run};
    Dispatch(kKinds, 2, Sc()[1], "BoneDartChild_Task");
}

// original 0x4A99C0: the dart, through BoneDartShaft_Steps (the last
// MAGIC058's 0x4AF490: the owner's count down, the task freed).
S09_EXPORT void __cdecl BoneDartShaft_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BoneDartShaft_Start, bof3::addr::BoneDartShaft_Fly,
                                                bof3::addr::BoneDartShaft_Bounce, bof3::addr::MagicFx_UncountAndFree};
    BoneDartChildRun(kSteps, "BoneDartShaft_Run");
}

// original 0x4A9A00: the owner's direction; (0x10000, 0) turned by it onto the
// owner's position, 0x100 above it; the sprite set up (fields +0x24..+0x2C,
// scale +0x40 / +0x44 0x10000, +0x48 1, no tint); animation 0; +9 0, +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x10000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) + U(ts->step_x));
        ts->z = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) + U(ts->step_z));
        rv::Store<unsigned>(ts->height_whole, (rv::As<records::SpriteHead>(Owner())->height_whole + 0x100u) & 0xFFFF);
        s[0x25] = 0x1D;
        s[0x26] = 0;
        rv::Store(ts->scale_x, 0x10000);
        rv::Store(ts->scale_y, 0x10000);
        s[0x48] = 1;
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(ts->column, 0);
        s[0x2B] = 1;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4A9B30: the script ticked; a step toward the source sprite's
// point (0x904B4C: its position and (height + 0xC0) / 2); near it (within
// 0x8000): the round's flag 0x2000, the target flagged 0x40, sound 0x203, the
// sprite's +0 bit 5 and tint mode 1 (no tint), the heading back to the owner
// (Math_Ratan2 of the owner's offset), +0x14 0x40 and +0x20 -8 (the rise and
// its pull), +9 0x10, +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Fly(void) {
    MH_CALL(Sprite_ScriptTick)();
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        const std::int32_t x = (rv::As<records::SpriteHead>(src)->x >> 9) - 0x4000;
        const std::int32_t z = (rv::As<records::SpriteHead>(src)->z >> 9) - 0x4000;
        const std::int32_t y = (S16(src + 0x3E) + 0xC0) >> 1;
        MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x30);
    }
    if (MH_CALL(MagicFx_NearSprite)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0x8000) == 0) return;
    const unsigned char target = TargetByte();
    Mem(::bof3::Img(at::kFlags) + 1)[0] |= 0x20;
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(Sound_PlayById)(0x203);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        s[0] |= 0x20;
        s[0x5C] = 1;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        const unsigned char* const o = Owner();
        const std::int32_t dz = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(o)->z) - U(ts->z));
        const std::int32_t dx = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(o)->x) - U(ts->x));
        const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        unsigned char* const t = Sc();
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->step_x = heading;
        rv::Store(tt->step_y, 0x40);
        rv::Store(tt->work_20, -8);
        t[9] = 0x10;
        Inc(t[2]);
    }
}

// original 0x4A9B30's sibling 0x4A9C60: the script ticked on odd frames; the
// tint 6 darker; along the heading +0xC by twice its sine and cosine (each
// address taken before its call); the rise +0x14 pulled by +0x20 and added to
// the height; +9 down, at 0 +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Bounce(void) {
    if (Frame_Counter & 1) MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        AddB(s[0x5D], 0xFA);
        AddB(s[0x5E], 0xFA);
        AddB(s[0x5F], 0xFA);
    }
    {
        unsigned char* const p = Sc() + 0x34;
        const int v = Sin(rv::Current<records::BattleTask>()->step_x);
        AddLong(p, U(ShlSar(v, 13, 12)));
    }
    {
        unsigned char* const p = Sc() + 0x38;
        const int v = Cos(rv::Current<records::BattleTask>()->step_x);
        AddLong(p, U(ShlSar(v, 13, 12)));
    }
    unsigned char* const s = Sc();
    AddLong(s + 0x14, U(rv::As<records::BattleTask>(s)->work_20));
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, (rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y)) & 0xFFFF);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4A9D10: the shadow, through BoneDartShadow_Steps (the last
// MAGIC058's 0x4AF490).
S09_EXPORT void __cdecl BoneDartShadow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BoneDartShadow_Start, bof3::addr::BoneDartShadow_Follow,
                                                bof3::addr::BoneDartShadow_Fade, bof3::addr::MagicFx_UncountAndFree};
    BoneDartChildRun(kSteps, "BoneDartShadow_Run");
}

// original 0x4A9D50: the owner's position and height; the sprite set up as
// the dart's but +0x2B 0, +0 bit 5 and tint mode 2 at 0xC0; animation 0; +9
// 0, +2 on.
S09_EXPORT void __cdecl BoneDartShadow_Start(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = rv::As<records::SpriteHead>(Owner())->x;
        ts->z = rv::As<records::SpriteHead>(Owner())->z;
        rv::Store<unsigned>(ts->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole);
        s[0x25] = 0x1D;
        s[0x26] = 0;
        rv::Store(ts->scale_x, 0x10000);
        rv::Store(ts->scale_y, 0x10000);
        s[0x48] = 1;
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
        s[0] |= 0x20;
        s[0x5C] = 2;
        s[0x5D] = 0xC0;
        s[0x5E] = 0xC0;
        s[0x5F] = 0xC0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(ts->column, 0);
        s[0x2B] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4A9E50: the script ticked; under the dart until its +2 is 2.
S09_EXPORT void __cdecl BoneDartShadow_Follow(void) {
    MH_CALL(Sprite_ScriptTick)();
    BoneDartShadowFollow(2);
}

// original 0x4A9F00: the script ticked on odd frames; the tint 3 darker;
// under the dart until its +2 is 3.
S09_EXPORT void __cdecl BoneDartShadow_Fade(void) {
    if (Frame_Counter & 1) MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        AddB(s[0x5D], 0xFD);
        AddB(s[0x5E], 0xFD);
        AddB(s[0x5F], 0xFD);
    }
    BoneDartShadowFollow(3);
}

void BoneDartShaft_Inject() {
    BOF3_INJECT(BoneDart_Task);
    BOF3_INJECT(BoneDart_Start);
    BOF3_INJECT(BoneDartChild_Task);
    BOF3_INJECT(BoneDartShaft_Run);
    BOF3_INJECT(BoneDartShaft_Start);
    BOF3_INJECT(BoneDartShaft_Fly);
    BOF3_INJECT(BoneDartShaft_Bounce);
    BOF3_INJECT(BoneDartShadow_Run);
    BOF3_INJECT(BoneDartShadow_Start);
    BOF3_INJECT(BoneDartShadow_Follow);
    BOF3_INJECT(BoneDartShadow_Fade);
}

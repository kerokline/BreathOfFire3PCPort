// Moved here by tools/move_funcs.py from battle_odds.cpp, magic_s37.cpp (docs/source-layout-round.md).
#include "game/battle/battle_fx_2.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_odds_callees.h"
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

using namespace battle_odds;

}  // namespace

namespace {

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U SpriteCurrent() { return Long(AtBattleOdds(battle_odds::at::Sprite_CurrentAt())); }

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS37 = move_script::Long;

using move_script::SetLong;

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t UMagicS37(std::int32_t v) { return static_cast<std::uint32_t>(v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(UMagicS37(LongMagicS37(at)) + v)); }

void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F7350: once +0xB is 0 - Battle_SetTargetFlag40(the byte
// kFlagTarget; pushed in ecx, whose upper bits are the entry's, and the callee
// reads the low byte), kRoundFlags |= 4, and a tail jump to
// BattleTask_FreeCurrent, whose eax is returned. Otherwise eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_Finish(void) {
    const U s = SpriteCurrent();
    if (AtBattleOdds(s + 0xB)[0] != 0) return s;
    g.set_flag40(AtBattleOdds(::bof3::Img(kFlagTarget.v))[0]);
    AtBattleOdds(::bof3::Img(kRoundFlags.v))[0] = static_cast<unsigned char>(AtBattleOdds(::bof3::Img(kRoundFlags.v))[0] | 4);
    return g.free_task();
}

// original 0x4F7380: the children's kind-1 task, a jmp through
// CombustionChild_Kinds (four: the sprite, the glow, a mote, the flash) by
// +1, unchecked.
S37_EXPORT void __cdecl CombustionChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {addr::CombustionSprite_Run, addr::CombustionGlow_Run,
                                                addr::CombustionMote_Run, addr::CombustionFlash_Run};
    Dispatch(kKinds, 4, Sc()[1], "CombustionChild_Task");
}

// original 0x4F73A0: with the frame-offset table the effects', a call through
// CombustionSprite_Steps (six: _Start, _Fall, _Shake, _Flash, MAGIC226/227's
// 0x4FA390, _Fade) by +2; then while bit 0 of +0 is set and +2 is not 0 the
// screen point and the sprite on the screen; the battle's table back.
S37_EXPORT void __cdecl CombustionSprite_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {addr::CombustionSprite_Start, addr::CombustionSprite_Fall,
                                                addr::CombustionSprite_Shake, addr::CombustionSprite_Flash,
                                                addr::MeteorStrikeRock_Hide,  addr::CombustionSprite_Fade};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 6, Sc()[2], "CombustionSprite_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4F73F0: +9 down; at 0 the sprite starts 0x7800000 over its
// owner's point, falling by 0x600000; its sprite fields, animation 0; +9 0x10,
// +2 on.
S37_EXPORT void __cdecl CombustionSprite_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, 0x7800000);
        rv::Store(rv::As<records::BattleTask>(s)->work_20, 0x600000);
        CopyLong(s + 0x34, Owner() + 0x34);
        CopyLong(s + 0x38, Owner() + 0x38);
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(UMagicS37(rv::As<records::SpriteHead>(Owner())->height) + UMagicS37(rv::As<records::BattleTask>(s)->step_y)));
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
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F7520: the height +0x14 down by the fall +0x20, set over the
// owner's; +9 down; at 0 sound 0x101, eight motes (kind 1, 0x60; +0x80 the
// owner, +1 2, +0xB the number; the owner's count up), the camera's first
// angle up by 0x14, +9 8, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Fall(void) {
    {
        unsigned char* const s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, static_cast<std::int32_t>(UMagicS37(rv::As<records::BattleTask>(s)->step_y) - UMagicS37(rv::As<records::BattleTask>(s)->work_20)));
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(UMagicS37(rv::As<records::SpriteHead>(Owner())->height) + UMagicS37(rv::As<records::BattleTask>(s)->step_y)));
        Dec(s[9]);
        if (s[9] != 0) return;
    }
    Sound(0x101);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x60);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(owner));
        child[1] = 2;
        child[0xB] = static_cast<unsigned char>(i);
        Inc(owner[0xB]);
    }
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    s[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F75E0: the camera's first angle up by 0x14 on odd +9, down on
// even; +9 down, at 0 the angle 0xFD56, +9 0x16, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Shake(void) {
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) != 0 ? 0x14 : -0x14));
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Sc()[9] = 0x16;
    Inc(Sc()[2]);
}

// original 0x4F7630: +9 down; at 0 sound 0x102, the flash (kind 1, 0x60;
// +0x80 the owner, +1 3; the owner's count up); the sprite's +0 bit 0x20,
// +0x5C 1, +0x5D..+0x5F 0x40; +9 0xC, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Flash(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sound(0x102);
    {
        const unsigned slot = NewTask(0x60);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(owner));
        child[1] = 3;
        Inc(owner[0xB]);
    }
    unsigned char* const s = Sc();
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x5C] = 1;
    s[0x5D] = 0x40;
    s[0x5E] = 0x40;
    s[0x5F] = 0x40;
    s[9] = 0xC;
    Inc(s[2]);
}

// original 0x4F76E0: +0x5D..+0x5F down by 0x10 each (add 0xF0), the scale
// +0x40 up by 0x1000 and again by 0x800 (+0x44 untouched); at +0x5D 0x80 the
// owner's count +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionSprite_Fade(void) {
    unsigned char* const s = Sc();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    AddLong(s + 0x40, 0x1000);
    AddLong(s + 0x40, 0x800);
    if (s[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

void BattleFx2_Inject() {
    BOF3_INJECT(BattleFx_Finish);
    BOF3_INJECT(CombustionChild_Task);
    BOF3_INJECT(CombustionSprite_Run);
    BOF3_INJECT(CombustionSprite_Start);
    BOF3_INJECT(CombustionSprite_Fall);
    BOF3_INJECT(CombustionSprite_Shake);
    BOF3_INJECT(CombustionSprite_Flash);
    BOF3_INJECT(CombustionSprite_Fade);
}

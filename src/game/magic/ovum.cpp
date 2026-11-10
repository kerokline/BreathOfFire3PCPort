// Moved here by tools/move_funcs.py from magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/ovum.h"

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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle (its kind)
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetBlast{0x8C5D80};

unsigned char* Sc() { return Sprite_Current; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4ABCA0: the kind-2 task. A three-entry stack table by +1:
// Ovum_Start, Ovum_Spawn, Ovum_End.
S10_EXPORT void __cdecl Ovum_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Ovum_Start, addr::Ovum_Spawn, addr::Ovum_End};
    Dispatch(kPhases, 3, Sc()[1], "Ovum_Task");
}

// original 0x4ABCD0: +9 8 (the children), +0xA 4 (the first delay), +1 on.
S10_EXPORT void __cdecl Ovum_Start(void) {
    Sc()[9] = 8;
    Sc()[0xA] = 4;
    Inc(Sc()[1]);
}

// original 0x4ABCF0: +0xA down; once it was 0, a child (kind 1, 0x50) owned
// by this task, +1 / +2 0, at the source sprite's screen point jittered by
// Rand (the first Rand's answer dropped), its +9 this task's +9; +9 down,
// and +0xA 8 until it was 0, then 0x2D and +1 on.
S10_EXPORT void __cdecl Ovum_Spawn(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char was = s[0xA];
        s[0xA] = static_cast<unsigned char>(was - 1);
        if (was != 0) return;
    }
    const unsigned slot = NewTask(0x50);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(Sc()));
    child[1] = 0;
    child[2] = 0;
    RandCall();
    const std::uint32_t r1 = RandCall();
    rv::Store<unsigned>(tchild->screen_x, (r1 & 0xF) + static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->screen_x));
    const std::uint32_t r2 = RandCall();
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store<unsigned>(tchild->screen_y, Word(src + 0x30) - (r2 & 0x1F));
    SetWord(child + 0x32, Word(src + 0x32));
    unsigned char* const s = Sc();
    child[9] = s[9];
    const unsigned char count = s[9];
    s[9] = static_cast<unsigned char>(count - 1);
    if (count != 0) {
        s[0xA] = 8;
        return;
    }
    s[0xA] = 0x2D;
    Inc(s[1]);
}

// original 0x4ABDD0: +0xA down; once it was 0, the target flagged 0x40, the
// effect's done flag, the task freed.
S10_EXPORT void __cdecl Ovum_End(void) {
    unsigned char* const s = Sc();
    const unsigned char was = s[0xA];
    s[0xA] = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ABE00: the child's kind-1 task, a one-entry stack table by +1.
S10_EXPORT void __cdecl OvumChild_Task(void) {
    static constexpr std::uint32_t kPhases[1] = {addr::OvumChild_Run};
    Dispatch(kPhases, 1, Sc()[1], "OvumChild_Task");
}

// original 0x4ABE20: with the frame-offset table 0x9039D8 the blast's
// (0x8C5D80), a three-entry stack table by +2 (OvumChild_Start, group S31's
// MainCannonBlast_Play, BattleFx_FreeTask); the screen update while +0 is
// set; the battle's table back.
S10_EXPORT void __cdecl OvumChild_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::OvumChild_Start, addr::MainCannonBlast_Play, addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBlast.v)));
    Dispatch(kSteps, 3, phase, "OvumChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4ABE80: the sprite fields (+0x24..+0x2D); sound 0x102 for the
// last child (+9 0), else 0x101 or 0x100 by +9's parity; animation 5; +2 on.
S10_EXPORT void __cdecl OvumChild_Start(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x27] = 0xB4;
    Sc()[0x28] = 0;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    const unsigned char n = Sc()[9];
    MH_CALL(Sound_PlayById)(static_cast<unsigned short>(n == 0 ? 0x102 : (n & 1) ? 0x101 : 0x100));
    MH_CALL(Sprite_SetAnimation)(5);
    Inc(Sc()[2]);
}

void Ovum_Inject() {
    BOF3_INJECT(Ovum_Task);
    BOF3_INJECT(Ovum_Start);
    BOF3_INJECT(Ovum_Spawn);
    BOF3_INJECT(Ovum_End);
    BOF3_INJECT(OvumChild_Task);
    BOF3_INJECT(OvumChild_Run);
    BOF3_INJECT(OvumChild_Start);
}

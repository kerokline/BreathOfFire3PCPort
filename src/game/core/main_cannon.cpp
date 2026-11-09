// Moved here by tools/move_funcs.py from magic_s31.cpp (docs/source-layout-round.md).
#include "game/core/main_cannon.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s31.h"
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

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

constexpr ::bof3::image::Va kFrameSetBlast{0x8C5D80};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S31_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The owner's script ticked with Sprite_Current the owner, Sprite_Current
// put back to the task it was (the originals keep it in a register).
void TickOwner(unsigned char* self) {
    Sprite_Current = Owner();
    MH_CALL(Sprite_ScriptTick)();
    Sprite_Current = self;
}

// (d << 4) / 8, toward zero.
std::int32_t Step8(int d) { return DivPow2(Shl(d, 4), 3); }

}  // namespace

// original 0x4E7FF0: the kind-2 task. A three-entry stack table by +1:
// MainCannon_Start, MainCannon_Fire, MainCannon_End.
S31_EXPORT void __cdecl MainCannon_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::MainCannon_Start, bof3::addr::MainCannon_Fire,
                                                 bof3::addr::MainCannon_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("MainCannon_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E8020: the screen point (0xF0, 0xFA); the first 16 words of CLUT
// row 26 back from the source (no STP bit); +0xB and +9 0; the owner's script
// ticked; +1 on.
S31_EXPORT void __cdecl MainCannon_Start(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, 0xF0);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, 0xFA);
    for (unsigned k = 0x1A00; k < 0x1A10; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    unsigned char* const self = Sc();
    TickOwner(self);
    Inc(self[1]);
}

// original 0x4E80A0: on every eighth frame: a shell (kind 1, 0x56, +1 0, +2 0)
// at the screen point (0xDC, 0xFA), 0x14 further right for an odd +9, with
// +0xA the shell's number +9 and +0x80 this task, +9 and +0xB up - none when
// no slot is free (0xFF); at +9 6 +1 on; then the owner's script ticked.
S31_EXPORT void __cdecl MainCannon_Fire(void) {
    if (Frame_Counter & 7) return;
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x56) & 0xFFu;
    if (slot != 0xFF) {
        unsigned char* const t = TaskSlot(slot);
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        t[1] = 0;
        t[2] = 0;
        unsigned char* const s = Sc();
        rv::Store<unsigned>(tt->screen_x, 0xDC);
        rv::Store<unsigned>(tt->screen_y, 0xFA);
        if (s[9] & 1) rv::Store<unsigned>(tt->screen_x, static_cast<std::uint16_t>(tt->screen_x) + 0x14u);
        t[0xA] = s[9];
        tt->owner = static_cast<std::int32_t>(Key(s));
        Inc(s[9]);
        Inc(Sc()[0xB]);
    }
    unsigned char* const s = Sc();
    if (s[9] == 6) Inc(s[1]);
    TickOwner(Sc());
}

// original 0x4E8160: the owner's script ticked; once every shell has ended
// (+0xB 0): the target flagged 0x40, the effect's done flag, the task freed.
S31_EXPORT void __cdecl MainCannon_End(void) {
    unsigned char* const self = Sc();
    TickOwner(self);
    if (self[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E81A0: the shells' kind-1 task, a jmp through
// MainCannonChild_Kinds (two entries: the shell, the blast) by +1, unchecked.
S31_EXPORT void __cdecl MainCannonChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::MainCannonShell_Run, bof3::addr::MainCannonBlast_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MainCannonChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4E81C0: with the frame-offset table 0x9039D8 the effects'
// (0x8E3580), a three-entry stack table by +2 (_Aim, _Fly, BattleFx_FreeTask);
// the sprite queued while bit 0 of +0 is set; the battle's table (0x8B3580)
// back.
S31_EXPORT void __cdecl MainCannonShell_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MainCannonShell_Aim, bof3::addr::MainCannonShell_Fly,
                                                bof3::addr::BattleFx_FreeTask};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MainCannonShell_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] & 1) MH_CALL(Sprite_QueueOverlay)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4E8220: the shell's step toward the acting actor (0x904B34: a
// party member's record at 0..2, else the enemy's by index - 3, unchecked):
// x (the actor's +0x2E - the shell's + 0x28, or + 0x14 for an even owner +9)
// x 2, y (the actor's +0x30 - the shell's) x 2, both in sixteenths; the
// position +0x18 / +0x1C in sixteenths; its sprite fields (+0x24..+0x2D,
// +0x48, +0x5C..+0x5F); animation (target - 1) & 1; +9 3, +2 on.
S31_EXPORT void __cdecl MainCannonShell_Aim(void) {
    const unsigned char actor = static_cast<unsigned char>(Long(Mem(::bof3::Img(at::kActor))));
    const unsigned char odd = Owner()[9] & 1;
    int ay;
    if (actor <= 2) {
        const int ax = S16(PartyRecord(actor) + 0x2E);
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = Step8(ax - S16(s + 0x2E) + (odd ? 0x28 : 0x14));
        ay = S16(PartyRecord(ActorIndex()) + 0x30);
    } else {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        const int ax = S16(EnemyRecord(actor) + 0x2E);
        ts->step_x = Step8(ax - S16(s + 0x2E) + (odd ? 0x28 : 0x14));
        ay = S16(EnemyRecord(ActorIndex()) + 0x30);
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = Step8(ay - S16(s + 0x30));
    }
    rv::Store(rv::Current<records::BattleTask>()->work_18, Shl(S16(Sc() + 0x2E), 4));
    rv::Store(rv::Current<records::BattleTask>()->work_1c, Shl(S16(Sc() + 0x30), 4));
    Sc()[0x25] = 0x1E;
    Sc()[0x26] = 0;
    Sc()[0x48] = 1;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 3;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>((TargetByte() - 1) & 1));
    Sc()[9] = 3;
    Inc(Sc()[2]);
}

// original 0x4E8440: the screen point from the position (sixteenths), the
// position on by the step; until the shell is within 0x14 (a party actor) or
// 0x28 (an enemy) above the actor's +0x30: a blast (kind 1, 0x56, +1 1, +2 0)
// at the shell's point with its +0xA and the owner as its +0x80, the shell's
// +2 on. Then the shell's script ticked.
S31_EXPORT void __cdecl MainCannonShell_Fly(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<unsigned>(rv::Current<records::BattleTask>()->work_18 >> 4) & 0xFFFF);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<unsigned>(rv::Current<records::BattleTask>()->work_1c >> 4) & 0xFFFF);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->work_18 = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_18) +
                                                    static_cast<std::uint32_t>(ts->step_x));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->work_1c = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_1c) +
                                                    static_cast<std::uint32_t>(ts->step_z));
    }
    const unsigned char actor = static_cast<unsigned char>(Long(Mem(::bof3::Img(at::kActor))));
    const int limit = actor <= 2 ? S16(PartyRecord(actor) + 0x30) - 0x14 : S16(EnemyRecord(actor) + 0x30) - 0x28;
    if (S16(Sc() + 0x30) < limit) {
        const unsigned slot = NewTask(0x56);
        unsigned char* const t = TaskSlot(slot);
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const s = Sc();
        t[1] = 1;
        t[2] = 0;
        rv::Store<unsigned>(tt->screen_x, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        rv::Store<unsigned>(tt->screen_y, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        t[0xA] = s[0xA];
        tt->owner = static_cast<std::int32_t>(Key(Owner()));
        Inc(s[2]);
    }
    MH_CALL(Sprite_ScriptTick)();
}

// original 0x4E8550: with the frame-offset table 0x9039D8 the blast's
// (0x8C5D80), a three-entry stack table by +2 (_Start, _Play, MAGIC058's
// 0x4AF490); the sprite's screen update while +0 is set; the battle's table
// back.
S31_EXPORT void __cdecl MainCannonBlast_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MainCannonBlast_Start, bof3::addr::MainCannonBlast_Play,
                                                bof3::addr::MagicFx_UncountAndFree};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBlast.v)));
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MainCannonBlast_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4E85B0: the blast's sprite fields (+0x24..+0x2D), animation 5,
// sound effect 0x100 + (+0xA & 1); +2 on.
S31_EXPORT void __cdecl MainCannonBlast_Start(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x27] = 0xB4;
    Sc()[0x28] = 0;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    MH_CALL(Sprite_SetAnimation)(5);
    MH_CALL(Sound_PlayEffect)(static_cast<unsigned short>((Sc()[0xA] & 1u) + 0x100));
    Inc(Sc()[2]);
}

// original 0x4E8640: the script ticked; at its end +2 on, else the sprite
// queued (a tail jmp).
S31_EXPORT void __cdecl MainCannonBlast_Play(void) {
    if (MH_CALL(Sprite_ScriptTick)() != 0) {
        Inc(Sc()[2]);
        return;
    }
    MH_CALL(Sprite_QueueOverlay)();
}

void MainCannon_Inject() {
    BOF3_INJECT(MainCannon_Task);
    BOF3_INJECT(MainCannon_Start);
    BOF3_INJECT(MainCannon_Fire);
    BOF3_INJECT(MainCannon_End);
    BOF3_INJECT(MainCannonChild_Task);
    BOF3_INJECT(MainCannonShell_Run);
    BOF3_INJECT(MainCannonShell_Aim);
    BOF3_INJECT(MainCannonShell_Fly);
    BOF3_INJECT(MainCannonBlast_Run);
    BOF3_INJECT(MainCannonBlast_Start);
    BOF3_INJECT(MainCannonBlast_Play);
}

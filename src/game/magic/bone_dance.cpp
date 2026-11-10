// Moved here by tools/move_funcs.py from battle_odds.cpp, magic_c2.cpp (docs/source-layout-round.md).
#include "game/magic/bone_dance.h"

#include <cstdint>
#include <cstring>

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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner))));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Key(const void* p) { return Low32(p); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

void AddLong(unsigned char* at, std::int32_t v) { SetLong(at, Add(Long(at), v)); }

int RandCall() { return MH_CALL(Rand)(); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter); }

void CopyPosition(unsigned char* to, const unsigned char* from) {
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's dispatch: the phase byte through its entries.
void Step(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The caster's animation, run with Sprite_Current its sprite and put back.
void CasterAnimation(unsigned animation) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current = self;
}

}  // namespace

#define C2_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The follower's position: the owner's x and z less one, the height the map's
// elevation at (x, x) - the original passes x twice (docs/magic_c2.md
// section 8).
void FollowOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, Add(rv::As<records::SpriteHead>(Owner())->x, -1));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(rv::As<records::SpriteHead>(Owner())->z, -1));
    const std::int32_t x = rv::Current<records::BattleTask>()->x;
    const long ground = MH_CALL(AreaMap_Elevation)(x, x);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(ground));
}

}  // namespace

// original 0x4AE7C0: the kind-2 task. A three-entry stack table by +1:
// BoneDance_Start, BoneDance_Run, BoneDance_End.
C2_EXPORT void __cdecl BoneDance_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDance_Start, addr::BoneDance_Run, addr::BoneDance_End};
    Step(kPhases, 3, Sc()[1], "BoneDance_Task");
}

// original 0x4AE7F0: the task at the owner's position; Gfx_ClutStripCopyRow(26),
// Gfx_ClutStripDirty up by one (read after the call); +1 on.
C2_EXPORT void __cdecl BoneDance_Start(void) {
    CopyPosition(Sc(), Owner());
    MH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    Inc(Sc()[1]);
}

// original 0x4AE850: a four-entry stack table by +2: BoneDance_Cast,
// BoneDance_Hold, BoneDance_Spawn, BoneDance_WaitBones.
C2_EXPORT void __cdecl BoneDance_Run(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::BoneDance_Cast, addr::BoneDance_Hold, addr::BoneDance_Spawn,
                                                 addr::BoneDance_WaitBones};
    Step(kPhases, 4, Sc()[2], "BoneDance_Run");
}

// original 0x4AE890: sounds 0x101 and 0x102; the caster's animation 3; the
// shaker (kind 1, parameter 0x41) with +1 = 1, +2 = 0 and this task its owner,
// the slot unchecked; +2 on.
C2_EXPORT void __cdecl BoneDance_Cast(void) {
    MH_CALL(Sound_PlayById)(0x101);
    MH_CALL(Sound_PlayById)(0x102);
    CasterAnimation(3);
    const unsigned slot = NewTask(0x41) & 0xFF;
    unsigned char* const self = Sc();
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    t[1] = 1;
    t[2] = 0;
    tt->owner = static_cast<std::int32_t>(Key(self));
    Inc(self[2]);
}

// original 0x4AE920: the caster's script ticked once (the answer unread); +9
// and +0xA 0. It never moves +2: the shaker does, after its third beat.
C2_EXPORT void __cdecl BoneDance_Hold(void) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    self[9] = 0;
    Sc()[0xA] = 0;
}

// original 0x4AE960: on odd frames, a bone (kind 1, parameter 0x41, +1 = 0) at
// the owner's position with this task its owner, counted in +9 - unless the
// pool is full (0xFF); at 0x20 bones +2 on instead.
C2_EXPORT void __cdecl BoneDance_Spawn(void) {
    if (!(Frame_Counter & 1)) return;
    unsigned char* const sc = Sc();
    if (sc[9] >= 0x20) {
        Inc(sc[2]);
        return;
    }
    const unsigned slot = NewTask(0x41) & 0xFF;
    if (slot == 0xFF) return;
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    t[1] = 0;
    t[2] = 0;
    const unsigned char* const owner = Owner();
    tt->x = rv::As<records::SpriteHead>(owner)->x;
    tt->z = rv::As<records::SpriteHead>(owner)->z;
    rv::Store(tt->height, rv::As<records::SpriteHead>(owner)->height);
    unsigned char* const self = Sc();
    tt->owner = static_cast<std::int32_t>(Key(self));
    Inc(self[9]);
}

// original 0x4AE9F0: every bone ended (+0xA, counted by BoneDanceBone_End, is
// +9): +1 on, +2 0.
C2_EXPORT void __cdecl BoneDance_WaitBones(void) {
    unsigned char* const sc = Sc();
    if (sc[9] != sc[0xA]) return;
    Inc(sc[1]);
    Sc()[2] = 0;
}

// original 0x4AEA20: the done flag, Battle_SetTargetFlag40(target), free.
C2_EXPORT void __cdecl BoneDance_End(void) {
    const unsigned target = Mem(::bof3::Img(magic_harness::at::kTarget))[0];
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4);
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AEA40: the child (kind 1, parameter 0x41). A three-entry stack
// table by +1: a bone, the shaker, the follower.
C2_EXPORT void __cdecl BoneDanceChild_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceBone_Run, addr::BoneDanceShake_Run,
                                                 addr::BoneDanceFollow_Run};
    Step(kPhases, 3, Sc()[1], "BoneDanceChild_Task");
}

// original 0x4AEA70: the frame-offset table 0x9039D8 at 0x8E3580; a
// three-entry stack table by +2 (start, fall, end); the sprite drawn while +0
// has bit 0; the table put back at 0x8B3580.
C2_EXPORT void __cdecl BoneDanceBone_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceBone_Start, addr::BoneDanceBone_Fall,
                                                 addr::BoneDanceBone_End};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    Step(kPhases, 3, Sc()[2], "BoneDanceBone_Run");
    if (Sc()[0] & 1) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4AEAD0: the bone placed round the owner - x by Rand % 6 + 2 (bit 0
// of +8) or Rand % 6 - 3, z by Rand % 6 + 3 or Rand % 6 + 2, in whole units
// (<< 16) - 0x600 higher, falling (+0x14 -32, +0x20 -1); its sprite fields; its
// animation 0; +9 3 (bounces); +2 on.
C2_EXPORT void __cdecl BoneDanceBone_Start(void) {
    int r = (Sc()[8] & 1) ? RandCall() % 6 + 2 : RandCall() % 6 - 3;
    rv::Store(rv::Current<records::BattleTask>()->x, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r) << 16), rv::As<records::SpriteHead>(Owner())->x));
    r = (Sc()[8] & 1) ? RandCall() % 6 + 3 : RandCall() % 6 + 2;
    rv::Store(rv::Current<records::BattleTask>()->z, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r) << 16), rv::As<records::SpriteHead>(Owner())->z));
    unsigned char* const s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + 0x600u);
    rv::Store(rv::As<records::BattleTask>(s)->step_y, -32);
    rv::Store(rv::As<records::BattleTask>(s)->work_20, -1);
    s[0x25] = 0x1D;
    s[0x26] = 0;
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
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 3;
    Inc(Sc()[2]);
}

// original 0x4AEC20: the height (+0x3E) down by +0x14's low word, +0x14 by
// +0x20; the sprite's script ticked; under the map's elevation at (x, z): a
// bounce - placed again round the owner (x by Rand % 6 + 2, z by Rand % 6 - 3,
// whatever +8), 0x600 higher, +0x14 -32 - and +9 down, at 0 +2 on.
C2_EXPORT void __cdecl BoneDanceBone_Fall(void) {
    unsigned char* s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
    s = Sc();
    AddLong(s + 0x14, rv::As<records::BattleTask>(s)->work_20);
    MH_CALL(Sprite_ScriptTick)();
    s = Sc();
    const long ground = MH_CALL(AreaMap_Elevation)(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->z);
    if (static_cast<short>(rv::Current<records::BattleTask>()->height_whole) >= static_cast<short>(ground)) return;
    int r = RandCall() % 6;
    rv::Store(rv::Current<records::BattleTask>()->x, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r + 2) << 16), rv::As<records::SpriteHead>(Owner())->x));
    r = RandCall() % 6;
    rv::Store(rv::Current<records::BattleTask>()->z, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r - 3) << 16), rv::As<records::SpriteHead>(Owner())->z));
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + 0x600u);
    rv::Store(rv::As<records::BattleTask>(s)->step_y, -32);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4AECF0: the owner's count of ended bones (+0xA) up; free.
C2_EXPORT void __cdecl BoneDanceBone_End(void) {
    Inc(Owner()[0xA]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AED00: a three-entry stack table by +2: BoneDanceShake_Start,
// BoneDanceShake_Step, BattleFx_FreeTask.
C2_EXPORT void __cdecl BoneDanceShake_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceShake_Start, addr::BoneDanceShake_Step,
                                                 addr::BattleFx_FreeTask};
    Step(kPhases, 3, Sc()[2], "BoneDanceShake_Run");
}

// original 0x4AED30: beat +9 = 0, its length +0xA from BoneDanceShake_Counts
// (read in place by +9); sound 0x100; the offset +0x10 0; +2 on.
C2_EXPORT void __cdecl BoneDanceShake_Start(void) {
    Sc()[9] = 0;
    unsigned char* const s = Sc();
    s[0xA] = Mem(::bof3::Img(BoneDanceShake_Counts_addr))[s[9]];
    MH_CALL(Sound_PlayById)(0x100);
    rv::Current<records::BattleTask>()->step_z = 0;
    Inc(Sc()[2]);
}

// original 0x4AED70: a two-entry stack table by +3 (down, up: the offset +0x10
// swings 0..-3); then, unless +0xA is 0xFF, the beat's length down - at 0 the
// next beat (+9 up, its length from the table, sound 0x100; at the third the
// owner's +2 on and +0xA 0xFF, else +3 0); when the owner has lost bit 0 of +0,
// the offset 0, +2 on, +3 0; every frame MapView_Redraw 2 and Camera_ShiftY
// the offset x 2.
C2_EXPORT void __cdecl BoneDanceShake_Step(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::BoneDanceShake_Down, addr::BoneDanceShake_Up};
    Step(kPhases, 2, Sc()[3], "BoneDanceShake_Step");
    unsigned char* s = Sc();
    if (s[0xA] != 0xFF) {
        Dec(s[0xA]);
        if (Sc()[0xA] == 0) {
            s = Sc();
            Inc(s[9]);
            s = Sc();
            s[0xA] = Mem(::bof3::Img(BoneDanceShake_Counts_addr))[s[9]];
            MH_CALL(Sound_PlayById)(0x100);
            s = Sc();
            if (s[9] == 3) {
                Inc(Owner()[2]);
                Sc()[0xA] = 0xFF;
            } else {
                s[3] = 0;
            }
        }
    }
    if (!(Owner()[0] & 1)) {
        rv::Current<records::BattleTask>()->step_z = 0;
        Inc(Sc()[2]);
        Sc()[3] = 0;
    }
    const std::uint16_t offset = Word(Sc() + 0x10);
    MapView_Redraw = 2;
    Camera_ShiftY = static_cast<short>(static_cast<std::uint16_t>(offset << 1));
}

// original 0x4AEE50: the offset +0x10 down while above -3; then +3 on.
C2_EXPORT void __cdecl BoneDanceShake_Down(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    const std::int32_t v = ts->step_z;
    if (v > -3)
        ts->step_z = Add(v, -1);
    else
        Inc(s[3]);
}

// original 0x4AEE70: the offset +0x10 up while below 0; then +3 back.
C2_EXPORT void __cdecl BoneDanceShake_Up(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    const std::int32_t v = ts->step_z;
    if (v < 0)
        ts->step_z = Add(v, 1);
    else
        Dec(s[3]);
}

// original 0x4AEE90 (PSX 0x800C2120): `jmp BattleTask_FreeCurrent` - the state
// that ends an effect, the running battle task's slot freed. 31 host tables in
// .text and one .data slot (0x64D474) hold it. eax the callee's.
extern "C" unsigned long __cdecl BattleFx_FreeTask(void) { return g.free_task(); }

// original 0x4AEEA0: the follower (+1 = 2; no creator in MAGIC057 sets it): the
// frame-offset table at 0x8E3580; a three-entry stack table by +2 (start,
// step, BattleFx_FreeTask); the sprite drawn while +0 has bit 0 and the
// owner's +0xA is 0; the table back at 0x8B3580.
C2_EXPORT void __cdecl BoneDanceFollow_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceFollow_Start, addr::BoneDanceFollow_Step,
                                                 addr::BattleFx_FreeTask};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    Step(kPhases, 3, Sc()[2], "BoneDanceFollow_Run");
    if ((Sc()[0] & 1) && Owner()[0xA] == 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4AEF00: at the owner (FollowOwner); the sprite fields as the
// bone's but grey (+0x5D..+0x5F 0x80); animation 0; +2 on.
C2_EXPORT void __cdecl BoneDanceFollow_Start(void) {
    FollowOwner();
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x48] = 1;
    s[0x27] = 0xA0;
    s[0x28] = 0;
    s[0x24] = 4;
    s[0x5D] = 0x80;
    s[0x5E] = 0x80;
    s[0x5F] = 0x80;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 4;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    MH_CALL(Sprite_SetAnimation)(0);
    Inc(Sc()[2]);
}

// original 0x4AEFE0: at the owner (FollowOwner); +2 on when the owner has lost
// bit 0 of +0.
C2_EXPORT void __cdecl BoneDanceFollow_Step(void) {
    FollowOwner();
    if (!(Owner()[0] & 1)) Inc(Sc()[2]);
}

void BoneDance_Inject() {
    BOF3_INJECT(BoneDance_Task);
    BOF3_INJECT(BoneDance_Start);
    BOF3_INJECT(BoneDance_Run);
    BOF3_INJECT(BoneDance_Cast);
    BOF3_INJECT(BoneDance_Hold);
    BOF3_INJECT(BoneDance_Spawn);
    BOF3_INJECT(BoneDance_WaitBones);
    BOF3_INJECT(BoneDance_End);
    BOF3_INJECT(BoneDanceChild_Task);
    BOF3_INJECT(BoneDanceBone_Run);
    BOF3_INJECT(BoneDanceBone_Start);
    BOF3_INJECT(BoneDanceBone_Fall);
    BOF3_INJECT(BoneDanceBone_End);
    BOF3_INJECT(BoneDanceShake_Run);
    BOF3_INJECT(BoneDanceShake_Start);
    BOF3_INJECT(BoneDanceShake_Step);
    BOF3_INJECT(BoneDanceShake_Down);
    BOF3_INJECT(BoneDanceShake_Up);
    BOF3_INJECT(BattleFx_FreeTask);
    BOF3_INJECT(BoneDanceFollow_Run);
    BOF3_INJECT(BoneDanceFollow_Start);
    BOF3_INJECT(BoneDanceFollow_Step);
}

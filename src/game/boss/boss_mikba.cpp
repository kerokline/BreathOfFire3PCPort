// Moved here by tools/move_funcs.py from boss_sg.cpp, boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_mikba.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_mikba_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/boss_sh.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_mikba::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's word - reads that word), its eax the dispatcher's.
using ForwardBossSg = U (__cdecl*)(unsigned);

ForwardBossSg EntryBossSg(U table, unsigned index) {
    return reinterpret_cast<ForwardBossSg>(static_cast<std::uintptr_t>(L(At(table + 4 * index))));   // as read: the fuzz swaps the cells
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSg(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryBossSg(table, state)(passed);
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the engine passes), a Fatal past
// them; the entry gets the caller's word whole.
U HookTableBossSg(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, word, (unsigned)table);
    return EntryBossSg(table, i)(word);
}

// A kind's entry: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8 (its sound words), 0x939AD8 read for each store.
void StoreKind(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
}

// The entry's end: Sprite_Current +1 = 2 (the idle step) and a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char Idle() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

using boss_harness::Handler;

using move_script::SetWord;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
unsigned char* PointerAt(U cell) { return At(static_cast<U>(Long(At(cell)))); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's word -
// reads that word), its eax the dispatcher's answer.
using EntryBossSh = U (__cdecl*)(U);

EntryBossSh EntryAtBossSh(U table, unsigned index) {
    return reinterpret_cast<EntryBossSh>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSh(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryAtBossSh(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the action pick, the hit and
// BattleEnemy_RunAll pass), the entry gets the word whole, a Fatal past them.
U HookDispatchBossSh(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAtBossSh(table, index)(word);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSh(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entrance, the one shape all seven kinds share (0x51 bytes each):
// 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook) and +0xF8 (its
// sound words), 0x939AD8 read for each store; then 0x939AD8 read once more and
// its dword +0x114 |= 8; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSh(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// AbilityList_Add as the originals call it, by its address (the jmp to ours
// in the game): the whole eax of its answer matters, because the second
// call's member word is that eax with its low byte replaced (`mov al,
// [0x675F08]` after the first call; AbilityList_Add's list lookup 0x591EC0
// masks the member to its byte, so the upper bytes are carried, never used).
using AbilityAdd = U (__cdecl*)(unsigned, unsigned, unsigned, unsigned);

// Set-up 34's two AbilityList_Add calls (its event hook's code 1 and its end
// hook): ability 0x40 to member 4's list (the third word 0: the member's own
// list, AbilityList_Add's reading), then to the member set-up 34 picked, the
// fourth word 1 - the member word the picked byte over the first answer's
// upper bytes.
void GiveAbility40() {
    const U first = BH_AT(AbilityAdd, bof3::addr::AbilityList_Add)(0x40, 4, 0, 0);
    BH_AT(AbilityAdd, bof3::addr::AbilityList_Add)(0x40, (first & 0xFFFFFF00u) | BossSh_Picked, 0, 1);
}

}  // namespace

// original 0x43DBF0: BossKind_Table[40]: jmp [BossMikba_States + 4 * +1] (12).
extern "C" U __cdecl BossMikba_Dispatch(unsigned passed) {
    return DispatchBossSg("BossMikba_Dispatch", AddressOf(BossMikba_States), 12, 1, passed);
}

// original 0x43DC10: state 0: BossMikba_Anims, BossMikba_Hook,
// BossMikba_Sounds; +1 = 2; tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMikba_Enter(void) {
    StoreKind(AddressOf(BossMikba_Anims), ::bof3::Code(bof3::addr::BossMikba_Hook), AddressOf(BossMikba_Sounds));
    return Idle();
}

// original 0x43DC50: state 6: jmp [BossMikba_ActSubs + 4 * +2] (6: the generic
// action entries with the kind's death dispatcher at 4).
extern "C" U __cdecl BossMikba_ActDispatch(unsigned passed) {
    return DispatchBossSg("BossMikba_ActDispatch", AddressOf(BossMikba_ActSubs), 6, 2, passed);
}

// original 0x43DC70: +2 entry 4: jmp [BossMikba_DeathSteps + 4 * +3] (7: the six
// steps below, then BossOp_ScriptTick).
extern "C" U __cdecl BossMikba_DeathDispatch(unsigned passed) {
    return DispatchBossSg("BossMikba_DeathDispatch", AddressOf(BossMikba_DeathSteps), 7, 3, passed);
}

// original 0x43DC90: death step 0. When Sprite_ScriptTick answers al not 0:
// the Sprite_Current of after +9 = 0x40, +0xA = 0x20, +3 = 1.
extern "C" void __cdecl BossMikba_DeathWait(void) {
    if ((BH_CALL(Sprite_ScriptTick)() & 0xFF) == 0) return;
    Sprite_Current[9] = 0x40;
    Sprite_Current[0xA] = 0x20;
    rv::Current<records::BattleEnemy>()->state3 = 1;
}

// original 0x43DCC0: death step 1, the flashes. With +9 equal to +0xA: unless
// 0x904AAD bit 2, Sound_PlayEffect(0x601) and 0x904AAD (read after) |= 4;
// BattleWin_DrawTileRgb(0, 0, 0xB, 0xFFFF, 1); the Sprite_Current of after
// +0xA halved (a byte). Otherwise +9 down by one. Then, +9 at 0 (the
// Sprite_Current of after), +3 up by one.
extern "C" void __cdecl BossMikba_DeathFlash(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count == s[0xA]) {
        if ((BossSe_Script & 4) == 0) {
            BH_CALL(Sound_PlayEffect)(0x601);
            BossSe_Script |= 4;
        }
        BH_CALL(BattleWin_DrawTileRgb)(0, 0, 0xB, 0xFFFF, 1);
        Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] >> 1);
    } else {
        s[9] = static_cast<unsigned char>(count - 1);
    }
    if (Sprite_Current[9] == 0) rv::Current<records::BattleEnemy>()->state3 = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state3 + 1);
}

// original 0x43DD30: death step 2. Sprite_ScriptTickOnce (answer not read);
// Gfx_ClearRect(0x340, 0x100, 0xC0, 0x100); the Sprite_Current of after: +0x18
// = the s16 +0x2E, +0x1C = the s16 +0x30, +0x20 = 0, +0xC = 0xBE, +0x10 = 0x68,
// the words +0x2E = 0x64 and +0x30 = 0x50, +0xA = 1, +0x24 |= 0x88;
// Sound_PlayEffect(0x602); BossMikba_DrawQuad(the Sprite_Current of after
// +9 - the original pushes it with the Sprite_Current's upper half above, which
// the draw never reads); the Sprite_Current of after +3 up by one.
extern "C" void __cdecl BossMikba_DeathOpen(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Gfx_ClearRect)(0x340, 0x100, 0xC0, 0x100);
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    rv::Store<std::int32_t>(cur_enemy->work_18, static_cast<std::int16_t>(static_cast<std::uint16_t>(cur_enemy->screen_x)));
    rv::Store<std::int32_t>(cur_enemy->work_1c, static_cast<std::int16_t>(static_cast<std::uint16_t>(cur_enemy->screen_y)));
    rv::Store<std::int32_t>(cur_enemy->work_20, 0);
    rv::Store<std::int32_t>(cur_enemy->step_x, 0xBE);
    rv::Store<std::int32_t>(cur_enemy->step_z, 0x68);
    cur_enemy->screen_x = 0x64;
    cur_enemy->screen_y = 0x50;
    s[0xA] = 1;
    s[0x24] |= 0x88;
    BH_CALL(Sound_PlayEffect)(0x602);
    BH_CALL(BossMikba_DrawQuad)(Sprite_Current[9]);
    rv::Current<records::BattleEnemy>()->state3 = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state3 + 1);
}

// original 0x43DDE0: death step 3. +0x20 += the byte +0xA; the s32 +0xC above
// 0x28: down by one, and on an odd Frame_Counter +0x1C up by one; the s32
// +0x10 above 0x28: down by one; BossMikba_DrawQuad(+0x20); then, the
// Sprite_Current of after: with Frame_Counter & 7 not 0, +0xA up by one; at
// +0xA 0x40 or above (a byte), +3 up by one.
extern "C" void __cdecl BossMikba_DeathSpread(void) {
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    rv::Store<std::int32_t>(cur_enemy->work_20, S(static_cast<U>(cur_enemy->work_20) + s[0xA]));
    const std::int32_t w = cur_enemy->step_x;
    if (w > 0x28) {
        rv::Store<std::int32_t>(cur_enemy->step_x, w - 1);
        if (Frame_Counter & 1) rv::Store<std::int32_t>(cur_enemy->work_1c, S(static_cast<U>(cur_enemy->work_1c) + 1u));
    }
    const std::int32_t h = cur_enemy->step_z;
    if (h > 0x28) rv::Store<std::int32_t>(cur_enemy->step_z, h - 1);
    BH_CALL(BossMikba_DrawQuad)(static_cast<U>(cur_enemy->work_20));
    if (Frame_Counter & 7) Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] + 1);
    if (Sprite_Current[0xA] >= 0x40) rv::Current<records::BattleEnemy>()->state3 = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state3 + 1);
}

// original 0x43DE60: death step 4. Sprite_SetAnimationBank(0x1C1); the
// Sprite_Current of after +0x2A = 1; Sprite_SetAnimation(8); the Sprite_Current
// of after +0x24 &= 0x77; Sprite_ScriptTick (answer not read);
// LoadDatFile(0xD0); the Sprite_Current of after +3 up by one.
extern "C" void __cdecl BossMikba_DeathLoad(void) {
    BH_CALL(Sprite_SetAnimationBank)(0x1C1);
    rv::Current<records::BattleEnemy>()->flip = 1;
    BH_CALL(Sprite_SetAnimation)(8);
    Sprite_Current[0x24] &= 0x77;
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(LoadDatFile)(0xD0);
    rv::Current<records::BattleEnemy>()->state3 = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state3 + 1);
}

// original 0x43DEA0: death step 5. Sprite_ScriptTick (answer not read); when
// File_LoadDone answers (its eax) not 0: Battle_EnemyDefeated, then the
// Sprite_Current of after +0 &= 0xBF, +1 = 6, +2 = 4, +3 = 6 (BossOp_ScriptTick
// from then on).
extern "C" void __cdecl BossMikba_DeathEnd(void) {
    BH_CALL(Sprite_ScriptTick)();
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Battle_EnemyDefeated)();
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    cur_enemy->flags &= 0xBF;
    cur_enemy->state = 6;
    cur_enemy->step = 4;
    cur_enemy->state3 = 6;
}

// original 0x43DEE0: the +0xF4 hook through BossMikba_Hooks (3, all BareRet).
extern "C" U __cdecl BossMikba_Hook(unsigned word) { return HookTableBossSg("BossMikba_Hook", AddressOf(BossMikba_Hooks), word); }

// original 0x43DEF0: BossKind_Table[48]: jmp [BossSample3_States + 4 * +1] (12:
// its entrance, then the generic enemy states).
extern "C" unsigned long __cdecl BossSample3_Dispatch(unsigned long through) {
    return DispatchBossSh("BossSample3_Dispatch", AddressOf(BossSample3_States), 12, 1, through);
}

// original 0x43DF10: state 0 - kind 40's (Mikba's) byte tables 0x64D6CC /
// 0x64D6D8 and its own hook BossSample3_Hook.
extern "C" unsigned char __cdecl BossSample3_Enter(void) {
    return EnterBossSh(::bof3::Img(BossMikba_Anims_addr), ::bof3::Code(bof3::addr::BossSample3_Hook), ::bof3::Img(BossMikba_Sounds_addr));
}

// original 0x43DF70: the +0xF4 hook through BossSample3_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample3_Hook(unsigned long word) {
    return HookDispatchBossSh("BossSample3_Hook", AddressOf(BossSample3_Hooks), word);
}

// original 0x43DF80: Boss_SetupTable[34]. The byte 0x669730 read once; each
// party member 0..2 whose +0x148 equals it leaves its index in 0x675F08 (the
// last one wins; none leaves the byte as it was). The hooks: end Boss34_End,
// exit Boss34_Exit, event Boss34_Event.
extern "C" void __cdecl Boss34_Setup(void) {
    const unsigned char id = Character_IndexById[4];
    for (unsigned m = 0; m <= 2; ++m)
        if (At(::bof3::Img(ObjTrio_addr) + 0x148 + m * boss_sh::at::kPartyStride)[0] == id) BossSh_Picked = static_cast<unsigned char>(m);
    StoreHooksBossSh(::bof3::Code(bof3::addr::Boss34_End), ::bof3::Code(bof3::addr::Boss34_Exit), ::bof3::Code(bof3::addr::Boss34_Event));
}

// original 0x43DFD0: the event hook, by the code's low byte; al 0 on every
// path.
//   5 (an action phase's): unless the round flags' bit 15, 0x904AAD bit 0, or
//     the picked member's +0x91 bit 0x20 (the member indexed by 0x675F08's
//     byte, unchecked - only set-up 34 writes it, 0..2): 0x904AAD |= 1 and
//     0x446700(the dword at 0x675F08) - the member put at the front of the
//     round's remaining order.
//   1 (an action phase's): with 0x904AAD bit 0 set and bit 1 clear and the
//     acting actor the picked member: the acting kind 0x904B35 = 4, the action
//     record (0x904B40) +1 = 4 and word +2 = 0x40 (the pointer read again),
//     the target = the picked member, 0x904AAD |= 2 (read after those
//     stores), the word 0x904B80 = 0x40 (stored before 0x904AAD); then
//     ability 0x40 twice (GiveAbility40).
//   any other: nothing.
extern "C" unsigned char __cdecl Boss34_Event(unsigned code) {
    switch (code & 0xFF) {
    case 5: {
        if (Long(At(::bof3::Img(at::kFlags.v))) & 0x8000) return 0;
        if (BossSe_Script & 1) return 0;
        const U picked = static_cast<U>(Long(At(::bof3::Img(BossSh_Picked_addr))));
        if (At(::bof3::Img(ObjTrio_addr) + 0x91 + (picked & 0xFF) * boss_sh::at::kPartyStride)[0] & 0x20) return 0;
        BossSe_Script = static_cast<unsigned char>(BossSe_Script | 1);
        BH_AT(void (__cdecl*)(unsigned), at::kOrderFront.v)(picked);
        return 0;
    }
    case 1: {
        const unsigned char script = BossSe_Script;
        if (script & 2) return 0;
        if ((script & 1) == 0) return 0;
        if (B(::bof3::Img(boss_sh::at::kActor.v)) != BossSh_Picked) return 0;
        unsigned char* const action = PointerAt(::bof3::Img(at::kAction.v));
        B(::bof3::Img(at::kActKind.v)) = 4;
        action[1] = 4;
        SetWord(PointerAt(::bof3::Img(at::kAction.v)) + 2, 0x40);
        B(::bof3::Img(boss_sh::at::kTarget.v)) = BossSh_Picked;
        const auto marked = static_cast<unsigned char>(BossSe_Script | 2);
        SetWord(At(::bof3::Img(at::kWord80.v)), 0x40);
        BossSe_Script = marked;
        GiveAbility40();
        return 0;
    }
    default: return 0;
    }
}

// original 0x43E0C0: the end hook. Won (0x904AE8 bit 1): for each party member
// 0..2 with +0 bit 0 - Battle_RemoveFromTurnOrder(its +5), then (its +0x90
// dword read after the call) Sprite_Current = the member and
// Sprite_PoseFromSet(its +8 + 0x1C when +0x90 bit 14, else + 4 (a byte),
// 0x8C5D80, 0x1800); then, unless 0x904AAD bit 1, ability 0x40 twice
// (GiveAbility40); 0x904AE8 |= 8; 0x446DE0 (the end phase, step 1) called and
// the chapter step 0x8034E5 incremented after it. Otherwise a tail jump to
// 0x446E00 (step 2).
extern "C" void __cdecl Boss34_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, boss_sh::at::kEndOther.v)();
        return;
    }
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * boss_sh::at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const auto pose = static_cast<unsigned char>(member_rec->direction + (flags & 0x4000 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(boss_sh::at::kPoseSet.v)), 0x1800);
    }
    if ((BossSe_Script & 2) == 0) GiveAbility40();
    Battle_EndFlags = static_cast<unsigned char>(Battle_EndFlags | 8);
    BH_AT(Handler, boss_sh::at::kEndWin.v)();
    MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
}

// original 0x43E210: the exit hook. BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0); Sprite_SetAnimationBank(0x1C1); Sprite_SetAnimation(8);
// then (Sprite_Current read again for each) +0x2A = 1, the words +0x58 / +0x5A
// = enemy 0's +0x58 / +0x5A - the actor tagged 0 put where enemy 0 stood. The
// answer is untested: with no actor tagged 0 the original writes through the
// null; ours aborts there.
extern "C" void __cdecl Boss34_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    Sprite_Current = BH_CALL(BossActor_Find)(0);
    BH_CALL(Sprite_SetAnimationBank)(0x1C1);
    BH_CALL(Sprite_SetAnimation)(8);
    if (Sprite_Current == nullptr)
        bof3::Fatal("Boss34_Exit: no field actor tagged 0 - the original writes +0x2A through the null (docs/boss_sh.md "
                    "section 6)");
    Sprite_Current[0x2A] = 1;
    SetWord(Sprite_Current + 0x58, BossSb_Enemy0Pose);
    SetWord(Sprite_Current + 0x5A, Battle_Enemies[0].frame_word);
}

// original 0x43E270: Boss_SetupTable[41]. The hooks: end BossHook_EndPickWay
// (BH's), exit BareRet, event BareRetZero.
extern "C" void __cdecl Boss41_Setup(void) {
    StoreHooksBossSh(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43E290: the death's quad, turned by the argument. One
// draw-mode primitive at Gfx_PacketNext (read after the page is made):
// Gpu_SetDrawMode(it, 0, 0, Gpu_GetTPage(2, 0, 0x340, 0x100) & 0xFFFF, 0),
// Gfx_CommitPrim(1, 0xC). Gte_PushMatrix; the rotation of the SVECTOR (0, 0,
// turn << 4) - Gte_RotMatrixYXZ into a matrix on the stack, Gte_SetRotMatrix;
// its translation zeroed (Gte_TransMatrix from a zero vector - the same three
// longs the corners' RotTrans writes into), Gte_SetTransMatrix. Then a PolyFT4
// at Gfx_PacketNext (read again): its tpage word +0x26 = Gpu_GetTPage(2, 0,
// 0x340, 0x100), colour 0x80 x3; four corners, each (x, y) = (-/+ +0xC / 2,
// -/+ +0x10 / 2) (s32 halved toward zero, then a short) in the order (-,-),
// (+,-), (-,+), (+,+) - with the SVECTOR's z left holding the turn, as the
// original's stack reuse has it - through Gte_RotTrans, the vertex floats x =
// Sprite_Current +0x18 + out x, y = +0x1C + out y - 0x28 (the Sprite_Current
// of after each RotTrans); the uv bytes (0, 0), (0xBF, 0), (0, 0x68), (0xBF,
// 0x68); Gpu_SetPolyFT4, Gfx_CommitPrim(1, 0x48), Gte_PopMatrix. The original's
// flag argument to RotTrans (a third push) is dropped: ours takes two.
extern "C" void __cdecl BossMikba_DrawQuad(unsigned turn) {
    const unsigned page = BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    BH_CALL(Gte_PushMatrix)();
    short vector[4] = {0, 0, static_cast<short>(turn << 4), 0};
    alignas(4) unsigned char matrix[32] = {};
    BH_CALL(Gte_RotMatrixYXZ)(vector, reinterpret_cast<short*>(matrix));
    BH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    long out[3] = {0, 0, 0};
    BH_CALL(Gte_TransMatrix)(reinterpret_cast<unsigned long*>(matrix), reinterpret_cast<const unsigned long*>(out));
    BH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    unsigned char* const prim = Gfx_PacketNext;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(prim)->tpage, BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));  // a POLY_FT4: Gpu_SetPolyFT4 below
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    auto half = [](const unsigned char* p) { return Long(p) / 2; };
    static constexpr int kSign[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    static constexpr unsigned kVertex[4] = {8, 0x18, 0x28, 0x38};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const s = Sprite_Current;
        vector[0] = static_cast<short>(kSign[i][0] * half(s + 0xC));
        vector[1] = static_cast<short>(kSign[i][1] * half(s + 0x10));
        BH_CALL(Gte_RotTrans)(vector, out);
        const float x = static_cast<float>(S(static_cast<U>(rv::Current<records::BattleEnemy>()->work_18) + static_cast<U>(out[0])));
        const float y = static_cast<float>(S(static_cast<U>(rv::Current<records::BattleEnemy>()->work_1c) + static_cast<U>(out[1]) - 0x28u));
        std::memcpy(prim + kVertex[i], &x, 4);
        std::memcpy(prim + kVertex[i] + 4, &y, 4);
    }
    prim[0x14] = 0;
    prim[0x15] = 0;
    prim[0x25] = 0;
    prim[0x24] = 0xBF;
    prim[0x34] = 0;
    prim[0x35] = 0x68;
    prim[0x44] = 0xBF;
    prim[0x45] = 0x68;
    BH_CALL(Gpu_SetPolyFT4)(prim);
    BH_CALL(Gfx_CommitPrim)(1, 0x48);
    BH_CALL(Gte_PopMatrix)();
}

void BossMikba_Inject() {
    BOF3_INJECT(BossMikba_Dispatch);
    BOF3_INJECT(BossMikba_Enter);
    BOF3_INJECT(BossMikba_ActDispatch);
    BOF3_INJECT(BossMikba_DeathDispatch);
    BOF3_INJECT(BossMikba_DeathWait);
    BOF3_INJECT(BossMikba_DeathFlash);
    BOF3_INJECT(BossMikba_DeathOpen);
    BOF3_INJECT(BossMikba_DeathSpread);
    BOF3_INJECT(BossMikba_DeathLoad);
    BOF3_INJECT(BossMikba_DeathEnd);
    BOF3_INJECT(BossMikba_Hook);
    BOF3_INJECT(BossSample3_Dispatch);
    BOF3_INJECT(BossSample3_Enter);
    BOF3_INJECT(BossSample3_Hook);
    BOF3_INJECT(Boss34_Setup);
    BOF3_INJECT(Boss34_Event);
    BOF3_INJECT(Boss34_End);
    BOF3_INJECT(Boss34_Exit);
    BOF3_INJECT(Boss41_Setup);
    BOF3_INJECT(BossMikba_DrawQuad);
}

// Moved here by tools/move_funcs.py from boss_sa.cpp, boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_weretigr.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_weretigr_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sa_callees.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_weretigr::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::AnswerHandler;

using boss_harness::Handler;

using boss_harness::Hook;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
template <typename T> U AddressOf(T* p) { return Low32(p); }

U Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through whatever follows). The handler's eax passes through
// the jmp; EnemyRunAll does not read it.
U DispatchBossSa(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return reinterpret_cast<AnswerHandler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)] - the handler gets
// the same word (the jmp leaves the caller's argument in place).
U HookDispatch(const char* who, U table, unsigned entries, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= entries)
        bof3::Fatal("%s: hook word %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, i, entries, (unsigned)table);
    return reinterpret_cast<Hook>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// A kind's state 0 (0x437A30's shape, five kinds): 0x939AD8's +0xFC (the
// animation bytes), +0xF4 (the hook), +0xF8 (the cue words) - 0x939AD8 read
// before each store -, Sprite_Current +1 = 2 (the idle step), and a tail jump
// to Sprite_ScriptTick (its al the answer).
void Install(U anims, U hook, U cues) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(anims);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(cues);
}

unsigned char EnterTick() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// Kinds 1 and 39's end: Sprite_ScriptTickOnce (its answer not read); then
// MoveCmd_OpE9 on the field actor tagged 0 (BossActor_Index(0), its object +
// 0x80) with (0, -14, 0x40, 0xFC00, 0xFF, 0); while it answers al not 0,
// nothing more. Then Battle_ClearActorBit(Sprite_Current +5) and 0x904AE8
// bit 1 (the win). Answers whether the walk is over.
bool EndWalk() {
    BH_CALL(Sprite_ScriptTickOnce)();
    const unsigned index = BH_CALL(BossActor_Index)(0);
    unsigned char* const actor = At(::bof3::Img(Sprite_Objects_addr) + 0x80 + index * at::kObjectStride);
    if (BH_CALL(MoveCmd_OpE9)(actor, 0, -0xE, 0x40, 0xFC00, 0xFF, 0) != 0) return false;
    BH_CALL(Battle_ClearActorBit)(Sprite_Current[5]);
    return true;
}

// The hooks' action-pick entry of kinds 1 and 39: an HP of 0xFFFF (the mark
// the hit entry leaves) makes the action's kind 3 and Sprite_Current +4 0.
bool PickSpent() {
    if (rv::As<records::BattleEnemy>(Enemy())->hp != 0xFFFF) return false;
    unsigned char* const s = Sprite_Current;
    B(::bof3::Img(at::kActionKind.v)) = 3;
    s[4] = 0;
    return true;
}

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSg(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

}  // namespace

// original 0x43D3D0: BossKind_Table[39]: jmp [BossWeretigr_Steps + 4 * +1] (12).
extern "C" U __cdecl BossWeretigr_Dispatch(void) {
    return DispatchBossSa("BossWeretigr_Dispatch", AddressOf(BossWeretigr_Steps), 12, 1);
}

// original 0x43D3F0: BossWeretigr_Steps 0 - +0xFC BossWeretigr_Anims, +0xF4
// BossWeretigr_Hook, +0xF8 BossWeretigr_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossWeretigr_Enter(void) {
    Install(AddressOf(BossWeretigr_Anims), ::bof3::Code(at::kWeretigrHook.v), AddressOf(BossWeretigr_Cues));
    return EnterTick();
}

// original 0x43D430: BossWeretigr_Steps 4 - jmp [BossWeretigr_State4Steps + 4
// * +2] (5).
extern "C" U __cdecl BossWeretigr_State4Dispatch(void) {
    return DispatchBossSa("BossWeretigr_State4Dispatch", AddressOf(BossWeretigr_State4Steps), 5, 2);
}

// original 0x43D450: BossWeretigr_State4Steps 0 - a slot n = BattleTask_Create(3,
// 6) (its al: the kind-3 dispatcher BattleBossFx_Dispatch's slot 6), and in
// it: the first 0x80 bytes of the acting actor's enemy object (0x93B960 +
// (0x904B34 - 3) * 0x128) copied a dword at a time (rep movsd), the kind +6 =
// 3, +5 = 6, the state bytes +1..+4 = 0, +0xB = n, the owner +0x80 =
// Sprite_Current. Then Sprite_Current +9 = n, +0xA = 0x10, +0 |= 0x40, +2 up
// by one.
extern "C" void __cdecl BossWeretigr_State4Fx(void) {
    const unsigned n = BH_CALL(BattleTask_Create)(3, 6) & 0xFF;
    if (n >= 48)
        bof3::Fatal("BossWeretigr_State4Fx: BattleTask_Create answered slot %u (all 48 taken) - the original copies into "
                    "0x93A000 + %u * 0x84, past the pool (docs/boss_sa.md section 6)",
                    n, n);
    unsigned char* const task = At(::bof3::Img(BattleTask_Slots_addr) + n * boss_sa::at::kTaskStride);
    const U from = ::bof3::Img(Battle_Enemies_addr) + static_cast<U>(static_cast<std::int32_t>(B(::bof3::Img(boss_sa::at::kActor.v))) - 3) * boss_sa::at::kEnemyStride;
    for (U i = 0; i < 0x80; i += 4) SetLong(task + i, Long(At(from + i)));
    task[6] = 3;
    task[5] = 6;
    task[1] = 0;
    task[2] = 0;
    task[3] = 0;
    task[4] = 0;
    unsigned char* const s = Sprite_Current;
    task[0xB] = static_cast<unsigned char>(n);
    SetLong(task + 0x80, static_cast<std::int32_t>(Key(s)));
    s[9] = static_cast<unsigned char>(n);
    Sprite_Current[0xA] = 0x10;
    Sprite_Current[0] |= 0x40;
    Sprite_Current[2] += 1;
}

// original 0x43D500: BossWeretigr_State4Steps 1 - +0xA down by one; at 0 the
// cue at 0x939AD8's +0xF8 (its first word) and that word + 4 through 0x437450
// (0x939AD8 and its +0xF8 read again for the second), +2 up by one.
extern "C" void __cdecl BossWeretigr_State4Cue(void) {
    Sprite_Current[0xA] -= 1;
    if (Sprite_Current[0xA] != 0) return;
    using Cue = void (__cdecl*)(unsigned);
    BH_AT(Cue, at::kPlayCue.v)(Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table)))));
    BH_AT(Cue, at::kPlayCue.v)((Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table)))) + 4u) & 0xFFFF);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x43D560: BossWeretigr_State4Steps 2 - once Sprite_Current +0 has
// lost bit 0x40 (the effect task clears it, by the step before's set):
// 0x904AA8 |= 4, 0x4376F0 (a chance of 0x904AA8 bit 7 and a task), and a tail
// jump to 0x4376A0 (the turn closed: +1 = 2, +2 = 0).
extern "C" void __cdecl BossWeretigr_State4End(void) {
    if (rv::Current<records::BattleEnemy>()->flags & 0x40) return;
    B(::bof3::Img(at::kRoundFlags.v)) |= 4;
    BH_AT(Handler, at::kTurnChance.v)();
    BH_AT(Handler, at::kTurnClose.v)();
}

// original 0x43D580: BossWeretigr_State4Steps 3 - Sprite_SetAnimation(0),
// Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossWeretigr_EndPose(void) {
    BH_CALL(Sprite_SetAnimation)(0);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x43D5A0: BossWeretigr_State4Steps 4 - the walk (EndWalk); when it
// is over, 0x904AE8 |= 2 (the win), the experience 0x904AEC += 0x939AD8's
// word +0x96 (both read after the calls), and a tail jump to Effect_Release.
//
// DIV-0045: that +0x96 times the EXP multiplier (src/game/cheats.cpp; 1
// unless BOF3X_EXP is set), as Battle_EnemyDefeated scales a fallen enemy's.
extern "C" void __cdecl BossWeretigr_EndMove(void) {
    if (!EndWalk()) return;
    const unsigned exp = rv::As<records::BattleEnemy>(Enemy())->exp * Cheats_ExpMultiplier();
    Battle_EndFlags |= 2;
    AddLong(At(::bof3::Img(Battle_ExpTotal_addr)), exp);
    BH_CALL(Effect_Release)();
}

// original 0x43D630: Weretigr's +0xF4 hook - jmp [BossWeretigr_Hooks + 4 *
// (word & 0xFF)] (3: BossWeretigr_HookPick, BossNue_HookHit, BareRet).
extern "C" U __cdecl BossWeretigr_Hook(unsigned word) {
    return HookDispatch("BossWeretigr_Hook", AddressOf(BossWeretigr_Hooks), 3, word);
}

// original 0x43D640: BossWeretigr_Hooks 0 (the action pick): with HP 0xFFFF,
// 0x904B35 = 3 and Sprite_Current +4 = 0 (BossNue_HookPick without its cue).
extern "C" void __cdecl BossWeretigr_HookPick(unsigned) { PickSpent(); }

// original 0x43D670: Boss_SetupTable[33]. The hooks: end Boss33_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss33_Setup(void) { StoreHooksBossSg(::bof3::Code(bof3::addr::Boss33_End), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x43D690: the end hook. With neither 0x904AE8 bit 1 nor bit 2: a
// tail jump to 0x446E00 (step 2). Otherwise BossActor_Clear(0); after it,
// Field_Kind2X = party member 0's +0x34 and Field_Kind2Z = its +0x38 (the
// dwords); the chapter's step = 8; a tail jump to 0x446DE0 (step 1).
extern "C" void __cdecl Boss33_End(void) {
    if ((Battle_EndFlags & 6) == 0) {
        BH_AT(Handler, boss_sg::at::kEndOther.v)();
        return;
    }
    BH_CALL(BossActor_Clear)(0);
    const std::int32_t x = ObjTrio[0].x;
    const std::int32_t z = ObjTrio[0].z;
    Field_Kind2X = x;
    Field_Kind2Z = z;
    MoveScript_PartyRecords[0x65] = 8;
    BH_AT(Handler, boss_sg::at::kEndWin.v)();
}

void BossWeretigr_Inject() {
    BOF3_INJECT(BossWeretigr_Dispatch);
    BOF3_INJECT(BossWeretigr_Enter);
    BOF3_INJECT(BossWeretigr_State4Dispatch);
    BOF3_INJECT(BossWeretigr_State4Fx);
    BOF3_INJECT(BossWeretigr_State4Cue);
    BOF3_INJECT(BossWeretigr_State4End);
    BOF3_INJECT(BossWeretigr_EndPose);
    BOF3_INJECT(BossWeretigr_EndMove);
    BOF3_INJECT(BossWeretigr_Hook);
    BOF3_INJECT(BossWeretigr_HookPick);
    BOF3_INJECT(Boss33_Setup);
    BOF3_INJECT(Boss33_End);
}

// Moved here by tools/move_funcs.py from boss_sa.cpp (docs/source-layout-round.md).
#include "game/boss/boss_01.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sa_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sa::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

void SetHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

}  // namespace

// original 0x437CD0: Boss_SetupTable[1] - BattleHook_End Boss01_End,
// BattleHook_Exit Boss01_Exit, BattleHook_Event Boss01_Event.
extern "C" void __cdecl Boss01_Setup(void) { SetHooks(::bof3::Code(at::kBoss01End.v), ::bof3::Code(at::kBoss01Exit.v), ::bof3::Code(at::kBoss01Event.v)); }

// original 0x437CF0: fight 1's event hook, by the phase code's low byte (a
// jump table of 7 inside the function); al 0 on every path.
//   0: with 0x904AA8 bit 0x40 and no target (0x904B44 0), 0x904AAD |= 1.
//   1: with the acting actor 0 (0x904B34), action kind 4 (0x904B35) and the
//      action's id (+2 of [0x904B40]) 0x78: both enemies' words +0xA4 and
//      +0xB0 = 1 (enemies 0 and 1), the leader's +0x134 bit 1 cleared.
//   5: with 0x904AAD bit 0: the leader's action id +0x126 = 0x78, its kind
//      +0x125 = 4 and 0x904B35 = 4, its target +0x124 = 0x40 and 0x904B44 =
//      0x40.
//   6: the leader's +0x134 bit 1 set.
//   2, 3, 4 and above 6: nothing.
extern "C" unsigned char __cdecl Boss01_Event(unsigned phase) {
    switch (phase & 0xFF) {
    case 0:
        if ((B(::bof3::Img(at::kRoundFlags.v)) & 0x40) && B(::bof3::Img(at::kTarget.v)) == 0) BossSe_Script |= 1;
        break;
    case 1:
        if (B(::bof3::Img(at::kActor.v)) == 0 && B(::bof3::Img(at::kActionKind.v)) == 4 && Word(At(BattleSprites_Action) + 2) == 0x78) {
            for (const U enemy : {::bof3::Img(Battle_Enemies_addr), ::bof3::Img(Battle_Enemies_addr) + 0x128}) {
                SetWord(At(enemy + 0xA4), 1);
                SetWord(At(enemy + 0xB0), 1);
            }
            SetLong(At(::bof3::Img(ObjTrio_addr) + 0x134), static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(ObjTrio_addr) + 0x134))) & ~2u));
        }
        break;
    case 5:
        if (BossSe_Script & 1) {
            SetWord(At(::bof3::Img(ObjTrio_addr) + 0x126), 0x78);
            rv::Bytes(ObjTrio)[0x125] = 4;
            B(::bof3::Img(at::kActionKind.v)) = 4;
            rv::Bytes(ObjTrio)[0x124] = 0x40;
            B(::bof3::Img(at::kTarget.v)) = 0x40;
        }
        break;
    case 6:
        SetLong(At(::bof3::Img(ObjTrio_addr) + 0x134), static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(ObjTrio_addr) + 0x134))) | 2u));
        break;
    default:
        break;
    }
    return 0;
}

// original 0x437DE0: fight 1's end hook. With 0x904AE8 bit 0 (the loss): the
// chapter's run 0x8034E4 = 6 and step 0x8034E5 = 0x32; otherwise the step up
// by one. Then a tail jump to 0x446E20 (the end phase, step 3) either way.
extern "C" void __cdecl Boss01_End(void) {
    if (Battle_EndFlags & 1) {
        MoveScript_Var7 = 6;
        MoveScript_PartyRecords[0x65] = 0x32;
    } else {
        MoveScript_PartyRecords[0x65] += 1;
    }
    BH_AT(Handler, at::kEndThird.v)();
}

// original 0x437E10: fight 1's exit hook. For the field actors tagged 6 and 7
// and enemies 0 and 1: BossActor_ClearBit40(tag), BossActor_CopyFrom(tag,
// the enemy's object, 1) (its place), Sprite_Current = BossActor_Find(tag),
// Sprite_SetAnimationBank(0x5F), the actor's +0x2A and +0x48 = 0,
// Sprite_SetAnimation(1 for tag 6, 0 for tag 7), then the actor's words
// +0x58 / +0x5A the enemy's - Sprite_Current read again at each store.
extern "C" void __cdecl Boss01_Exit(void) {
    const struct { unsigned tag; U enemy; unsigned animation; } kActors[] = {{6, ::bof3::Img(Battle_Enemies_addr), 1}, {7, ::bof3::Img(Battle_Enemies_addr) + 0x128, 0}};
    for (const auto& a : kActors) {
        BH_CALL(BossActor_ClearBit40)(a.tag);
        BH_CALL(BossActor_CopyFrom)(a.tag, At(a.enemy), 1);
        Sprite_Current = BH_CALL(BossActor_Find)(a.tag);
        BH_CALL(Sprite_SetAnimationBank)(0x5F);
        Sprite_Current[0x2A] = 0;
        Sprite_Current[0x48] = 0;
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(a.animation));
        SetWord(Sprite_Current + 0x58, Word(At(a.enemy + 0x58)));
        SetWord(Sprite_Current + 0x5A, Word(At(a.enemy + 0x5A)));
    }
}

void Boss01_Inject() {
    BOF3_INJECT(Boss01_Setup);
    BOF3_INJECT(Boss01_Event);
    BOF3_INJECT(Boss01_End);
    BOF3_INJECT(Boss01_Exit);
}

// Moved here by tools/move_funcs.py from boss_se.cpp, boss_sf.cpp (docs/source-layout-round.md).
#include "game/boss/boss_25.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_25_callees.h"
#include "game/boss_harness.h"
#include "game/boss_se_callees.h"
#include "game/boss_sf_callees.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "game/solo/solo_scenes.h"  // DIV-0228: the scene guards

namespace {

namespace at = boss_25::at;

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

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSe(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// Msg_OpenScript(id) with the byte 0x802D20 = 2 first, as every message here.
void Message(unsigned id) {
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSf(U end, U exit, U event) {
    BattleHook_End = end;
    BattleHook_Exit = exit;
    BattleHook_Event = event;
}

}  // namespace

// original 0x43BEA0: Boss_SetupTable[25]. Party member 0's HP +0x98 and AP
// +0x9A saved - their LOW BYTES only - in 0x675F05 / 0x675F04, and the two words
// set from its +0xA0 / +0xA2; the hooks: end Boss25_End, exit Boss25_Exit,
// event Boss25_Event; 0x904AE4 = 3. In the original's order (every read of a
// cell before its store).
extern "C" void __cdecl Boss25_Setup(void) {
    const unsigned char hp = rv::Bytes(ObjTrio)[0x98];
    const unsigned max_hp = ObjTrio[0].max_hp;
    const unsigned char ap = rv::Bytes(ObjTrio)[0x9a];
    BossSe_SavedHp = hp;
    const unsigned max_ap = ObjTrio[0].max_ap;
    StoreHooksBossSe(::bof3::Code(bof3::addr::Boss25_End), ::bof3::Code(bof3::addr::Boss25_Exit), ::bof3::Code(bof3::addr::Boss25_Event));
    BattlePhases_Initiative = 3;
    ObjTrio[0].hp = max_hp;
    BossSe_SavedAp = ap;
    ObjTrio[0].ap = max_ap;
}

// original 0x43BF00: the event hook, by the code's low byte; al 0 on every
// path.
//   0 (an action phase's): the turn counter 0x904B90 = (0x904AE2 >> 1) + 1;
//     by 0x904AE2 - 1: message 0x1B; 2: 0x802D20 = 2, member 0's action +0x125
//     = 1, message 0x1C; 3: message 0x1D; 4 (unless round-flag bit 6): member
//     0's action = 4 and skill +0x126 = 0x46; 5: message 0x1E; 6 (unless bit
//     6): message 0x1F, then member 0's action = 1, +0xBA = 0x64, enemy 0's HP
//     = 1.
//   3 (Battle_PhaseDispatch's): 0x904AAD's bits in turn -
//     0x20 clear and phase 1: BattleTask_Create(0, 8), bit 0x20 set;
//     0x10 clear, phase 3, step 0: enemy 0's +0x110 |= 2 and +0x104 = 0, +0x105
//       = 2; member 0's target +0x124 = 3, action 1, +0xBC = 0x64, +0xBA = 0; the
//       turn order 0x904ACC = 0, 3, 0, 3, 0, 3, 0 (7); bit 0x10 set (on the
//       value held);
//     then the first of bits 1, 2, 4 clear: message 0x19 / enemy 0 made
//       Sprite_Current with BattleEnemy_SetAnimation(8) / message 0x1A, and that
//       bit set (0x904AAD read again after the call); each returns;
//     all set: 0x904AE2 4 with member 0's +1 at 6 and +2 at 1 - its words
//       +0x128 and +0x12A = 0 and the acting kind 0x904B35 = 0xFF; 0x904AE2 4 or
//       6 with member 0's +1 at 2 and +2 at 0 - round-flag bit 6 set.
//   any other: nothing.
extern "C" unsigned char __cdecl Boss25_Event(unsigned code) {
    const unsigned char c = static_cast<unsigned char>(code);
    if (c == 0) {
        const unsigned char round = BossSe_RoundSlot;
        SetLong(At(::bof3::Img(boss_se::at::kTurn.v)), S((static_cast<U>(round) >> 1) + 1));
        switch (round) {
        case 1: Message(0x1B); break;
        case 2:
            BossSa_WindowPass = round;
            rv::Bytes(ObjTrio)[0x125] = 1;
            BH_CALL(Msg_OpenScript)(0x1C);
            break;
        case 3: Message(0x1D); break;
        case 4:
            if (B(::bof3::Img(at::kFlags.v)) & 0x40) break;
            rv::Bytes(ObjTrio)[0x125] = 4;
            SetWord(At(::bof3::Img(ObjTrio_addr) + 0x126), 0x46);
            break;
        case 5: Message(0x1E); break;
        case 6:
            if (B(::bof3::Img(at::kFlags.v)) & 0x40) break;
            Message(0x1F);
            rv::Bytes(ObjTrio)[0x125] = 1;
            rv::Bytes(ObjTrio)[0xba] = 0x64;
            Battle_Enemies[0].hp = 1;
            break;
        default: break;
        }
        return 0;
    }
    if (c != 3) return 0;
    unsigned char script = BossSe_Script;
    if ((script & 0x20) == 0 && B(::bof3::Img(boss_se::at::kPhase.v)) == 1) {
        BH_CALL(BattleTask_Create)(0, 8);
        script = static_cast<unsigned char>(BossSe_Script | 0x20);
        BossSe_Script = script;
    }
    if ((script & 0x10) == 0 && B(::bof3::Img(boss_se::at::kPhase.v)) == 3 && BattleActions_Step == 0) {
        const U flags = Battle_Enemies[0].flags_110 | 2;
        rv::Bytes(ObjTrio)[0x124] = 3;
        rv::Bytes(ObjTrio)[0x125] = 1;
        rv::Bytes(ObjTrio)[0xbc] = 0x64;
        rv::Bytes(ObjTrio)[0xba] = 0;
        Battle_Enemies[0].command = 0;
        Battle_Enemies[0].flags_110 = S(flags);
        Battle_Enemies[0].command_kind = 2;
        BattleDamage_TurnCount = 7;
        static constexpr unsigned char kOrder[7] = {0, 3, 0, 3, 0, 3, 0};
        for (unsigned i = 0; i < 7; ++i) B(::bof3::Img(at::kOrder.v) + i) = kOrder[i];
        script = static_cast<unsigned char>(script | 0x10);
        BossSe_Script = script;
    }
    if ((script & 1) == 0) {
        Message(0x19);
        BossSe_Script |= 1;
        return 0;
    }
    if ((script & 2) == 0) {
        Sprite_Current = At(::bof3::Img(Battle_Enemies_addr));
        BH_CALL(BattleEnemy_SetAnimation)(8);
        BossSe_Script |= 2;
        return 0;
    }
    if ((script & 4) == 0) {
        Message(0x1A);
        BossSe_Script |= 4;
        return 0;
    }
    const unsigned char round = BossSe_RoundSlot;
    unsigned char member;
    if (round == 4) {
        member = ObjTrio[0].state;
        if (member == 6) {
            if (ObjTrio[0].step != 1) return 0;
            SetWord(At(::bof3::Img(ObjTrio_addr) + 0x128), 0);
            SetWord(At(::bof3::Img(ObjTrio_addr) + 0x12a), 0);
            B(::bof3::Img(boss_se::at::kActKind.v)) = 0xFF;
            return 0;
        }
    } else if (round == 6) {
        member = ObjTrio[0].state;
    } else {
        return 0;
    }
    if (member != 2 || ObjTrio[0].step != 0) return 0;
    B(::bof3::Img(at::kFlags.v)) |= 0x40;
    return 0;
}

// original 0x43C180: the end hook. Member 0's HP and AP words = the two saved
// bytes (zero-extended); the chapter's step 0x8034E5 = 4; a tail jump to
// 0x446E20 (step 3).
extern "C" void __cdecl Boss25_End(void) {
    const unsigned hp = BossSe_SavedHp;
    const unsigned ap = BossSe_SavedAp;
    ObjTrio[0].hp = hp;
    ObjTrio[0].ap = ap;
    MoveScript_PartyRecords[0x65] = 4;
    BH_AT(Handler, boss_se::at::kEndThird.v)();
}

// original 0x43C1B0: the exit hook. Round-flag bit 1 set; Transition_Start(4);
// then the count byte 0x939A02 read, Field_MemberCount = 3, and that many bytes
// copied from 0x939A10 to the party list 0x904065 (rep movsd, rep movsb:
// forward, the two never overlap).
extern "C" void __cdecl Boss25_Exit(void) {
    B(::bof3::Img(at::kFlags.v)) |= 2;
    BH_CALL(Transition_Start)(4);
    const unsigned n = BossSe_PartyCopyCount;
    // DIV-0228 SoloCharacter: the count chapter 6's SortParty saved (0x939A02)
    // goes back, not 3, when it saved fewer than three walkers (the fold's
    // team); with three the two are the same (solo_scenes.h). Off: 3.
    Field_MemberCount = solo::Scene_Active() && n < 3 ? static_cast<unsigned char>(n) : 3;
    if (n != 0) std::memcpy(At(::bof3::Img(Cond_Flags_addr) + 0xd5), At(::bof3::Img(at::kPartyCopyFrom.v)), n);
}

// original 0x43C200: Boss_SetupTable[26]. The hooks: end Boss26_End, exit
// BossHook_ExitTransition4, event Boss26_Event; then the two sums 0x939A08 and
// 0x939A04 and the count 0x939A0C = 0.
extern "C" void __cdecl Boss26_Setup(void) {
    StoreHooksBossSe(::bof3::Code(bof3::addr::Boss26_End), ::bof3::Code(bof3::addr::BossHook_ExitTransition4), ::bof3::Code(bof3::addr::Boss26_Event));
    BattleE1_SumB = 0;
    BattleE1_SumA = 0;
    BattleE1_Count = 0;
}

// original 0x43C230: the event hook, by the code's low byte (a jump table of
// four inside the function); al 0 on every path.
//   0: enemy 0 acting (0x904B34 3) on a member (the target 0x904B44 0..2) whose
//      s16 +0x128 is above 0 - that added to the sum 0x939A08. Otherwise, the
//      target enemy 0 (3) or a side (bit 6 or 7): with enemy 0's +0x110 bit 1
//      the count 0x939A0C up by one, else its s16 +0x108, when above 0, added
//      to the sum 0x939A04.
//   1: 0x904AE2 at 1 and enemy 0's +0x8E at 1 - enemy 0's +0x110 |= 2, +1 =
//      2, +2 = 0, +0x105 = 2.
//   2: the turn counter 0x904B90 at 0x15 - 0x904AE8 |= 4.
//   3: 0x904AAD bit 5 clear and phase 1 - BattleTask_Create(0, 8) and bit 5 set
//      (0x904AAD read again after the call).
//   above 3: nothing.
extern "C" unsigned char __cdecl Boss26_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: {
        const unsigned char actor = B(::bof3::Img(at::kActor.v));
        const unsigned char target = B(::bof3::Img(boss_se::at::kTarget.v));
        if (actor == 3 && target <= 2) {
            const auto w = static_cast<std::int16_t>(Word(At(::bof3::Img(ObjTrio_addr) + 0x128 + boss_se::at::kPartyStride * target)));
            if (w > 0) {
                BattleE1_SumB = S(BattleE1_SumB + static_cast<U>(static_cast<std::int32_t>(w)));
                return 0;
            }
        }
        if (target != 3 && (target & 0xC0) == 0) return 0;
        if (B(::bof3::Img(at::kEnemy0Flags.v)) & 2) {
            BattleE1_Count = static_cast<unsigned char>(BattleE1_Count + 1);
            return 0;
        }
        const auto v = Battle_Enemies[0].hp_change;
        if (v > 0)
            BattleE1_SumA = S(BattleE1_SumA + static_cast<U>(static_cast<std::int32_t>(v)));
        return 0;
    }
    case 1:
        if (BossSe_RoundSlot == 1 && Battle_Enemies[0].odds_index == 1) {
            const U flags = Battle_Enemies[0].flags_110 | 2;
            Battle_Enemies[0].state = 2;
            Battle_Enemies[0].flags_110 = S(flags);
            Battle_Enemies[0].step = 0;
            Battle_Enemies[0].command_kind = 2;
        }
        return 0;
    case 2:
        if (static_cast<U>(Long(At(::bof3::Img(boss_se::at::kTurn.v)))) == 0x15) Battle_EndFlags |= 4;
        return 0;
    case 3:
        if ((BossSe_Script & 0x20) == 0 && B(::bof3::Img(boss_se::at::kPhase.v)) == 1) {
            BH_CALL(BattleTask_Create)(0, 8);
            BossSe_Script |= 0x20;
        }
        return 0;
    default: return 0;
    }
}

// original 0x43C370: the end hook. For each party member 0..2 (ObjTrio) with
// +0 bit 0: Battle_RemoveFromTurnOrder(its +5); its +0x90 read after the call;
// Sprite_Current = the member; Sprite_PoseFromSet(its +8 + 0x1C with +0x90 bit
// 14, else + 4 - a byte, 0x8C5D80, 0x1800). Then variable 3 = 4 and a tail jump
// to 0x446E20 (step 3).
extern "C" void __cdecl Boss26_End(void) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * boss_se::at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(member_rec->direction + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(boss_se::at::kPoseSet.v)), 0x1800);
    }
    B(::bof3::Img(boss_se::at::kScriptVar3.v)) = 4;
    BH_AT(Handler, boss_se::at::kEndThird.v)();
}

// original 0x43C480: Boss_SetupTable[27]. The hooks: end Boss27_End, exit
// Boss27_Exit, event Boss27_Event.
extern "C" void __cdecl Boss27_Setup(void) { StoreHooksBossSf(::bof3::Code(bof3::addr::Boss27_End), ::bof3::Code(bof3::addr::Boss27_Exit), ::bof3::Code(bof3::addr::Boss27_Event)); }

// original 0x43C4A0: the event hook, by the code's low byte; al 0 on every
// path.
//   0: enemy 0's +0x92 bit 0x4000 - the script bits 0x904AAD |= 1 and
//      0x904AE8 |= 4; else enemy 1's - 0x904AAD |= 2 and 0x904AE8 |= 4; else,
//      with 0x904AE8 0 and 0x904AAD bit 3 clear, the actor before the turn
//      order's cursor (0x904ACB + 0x904AE2): a member (0..2) whose +0x130 has
//      neither bit 0 nor 1 - 0x904AE8 = 4 and 0x904AAD |= 4. Then (those two
//      paths too) 0x904AAD bit 5 clears bits 5 and 3.
//   2: without 0x904AE8 bit 2, with 0x904AAD bit 4 - bit 4 cleared,
//      Sprite_Current = enemy 1, Sprite_EnsureAnimation(0xC), then 0x904AAD
//      (read again) |= 8.
//   others: nothing.
// The original keeps the actor byte in its own argument slot ([esp + 4]); no
// caller reads it back.
extern "C" unsigned char __cdecl Boss27_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: {
        if (static_cast<U>(Long(At(::bof3::Img(Battle_Enemies_addr) + 0x92))) & 0x4000) {
            BossSe_Script |= 1;
            Battle_EndFlags |= 4;
            return 0;
        }
        if (BossH_Enemy1Status & 0x4000) {
            BossSe_Script |= 2;
            Battle_EndFlags |= 4;
            return 0;
        }
        if (Battle_EndFlags == 0 && (BossSe_Script & 8) == 0) {
            const unsigned char who = B(::bof3::Img(at::kOrderBefore.v) + BossSe_RoundSlot);
            if (who <= 2 && (At(::bof3::Img(ObjTrio_addr) + who * boss_sf::at::kPartyStride)[0x130] & 3) == 0) {
                const unsigned char script = BossSe_Script;
                Battle_EndFlags = 4;
                BossSe_Script = static_cast<unsigned char>(script | 4);
            }
        }
        const unsigned char script = BossSe_Script;
        if (script & 0x20) BossSe_Script = static_cast<unsigned char>(script & 0xD7);
        return 0;
    }
    case 2: {
        if (Battle_EndFlags & 4) return 0;
        const unsigned char script = BossSe_Script;
        if ((script & 0x10) == 0) return 0;
        BossSe_Script = static_cast<unsigned char>(script & 0xEF);
        Sprite_Current = At(::bof3::Img(Battle_Enemies_addr) + 0x128);
        BH_CALL(Sprite_EnsureAnimation)(0xC);
        BossSe_Script |= 8;
        return 0;
    }
    default: return 0;
    }
}

// original 0x43C5C0: the end hook. For each party member 0..2 (ObjTrio) with
// +0 bit 0: Battle_RemoveFromTurnOrder(its +5); its +0x90 read after the call;
// Sprite_Current = the member; Sprite_PoseFromSet(its +8 + 0x1C with +0x90 bit
// 14, else + 4 - a byte, 0x8C5D80, 0x1800) - set-up 26's loop (Boss26_End).
// Then the script bits 0x904AAD, read once: bit 0 the chapter's step 0xF, bit
// 1 step 7, bit 2 step 0x14 (each stored in turn, the last set bit's
// standing); a tail jump to 0x446E20 (step 3).
extern "C" void __cdecl Boss27_End(void) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * boss_sf::at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(member_rec->direction + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(boss_sf::at::kPoseSet.v)), 0x1800);
    }
    const unsigned char script = BossSe_Script;
    if (script & 1) MoveScript_PartyRecords[0x65] = 0xF;
    if (script & 2) MoveScript_PartyRecords[0x65] = 7;
    if (script & 4) MoveScript_PartyRecords[0x65] = 0x14;
    BH_AT(Handler, boss_sf::at::kEndThird.v)();
}

// original 0x43C6F0: the exit hook. With 0x904AAD bit 0,
// BossActor_CopyFrom(1, enemy 0, 0) (its pose); BossActor_ClearBit40(1); with
// 0x904AAD (read again) bit 1, BossActor_CopyFrom(2, enemy 1, 0);
// BossActor_ClearBit40(2).
extern "C" void __cdecl Boss27_Exit(void) {
    if (BossSe_Script & 1) BH_CALL(BossActor_CopyFrom)(1, At(::bof3::Img(Battle_Enemies_addr)), 0);
    BH_CALL(BossActor_ClearBit40)(1);
    if (BossSe_Script & 2) BH_CALL(BossActor_CopyFrom)(2, At(::bof3::Img(Battle_Enemies_addr) + 0x128), 0);
    BH_CALL(BossActor_ClearBit40)(2);
}

void Boss25_Inject() {
    BOF3_INJECT(Boss25_Setup);
    BOF3_INJECT(Boss25_Event);
    BOF3_INJECT(Boss25_End);
    BOF3_INJECT(Boss25_Exit);
    BOF3_INJECT(Boss26_Setup);
    BOF3_INJECT(Boss26_Event);
    BOF3_INJECT(Boss26_End);
    BOF3_INJECT(Boss27_Setup);
    BOF3_INJECT(Boss27_Event);
    BOF3_INJECT(Boss27_End);
    BOF3_INJECT(Boss27_Exit);
}

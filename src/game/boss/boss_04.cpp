// Moved here by tools/move_funcs.py from boss_sb.cpp (docs/source-layout-round.md).
#include "game/boss/boss_04.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sb::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

void Setup(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

// The event hooks' shared cases (the same code in B04, B05, B06 and B07).
// Phase 0: with round-flag bit 0x40 and the target 0, 0x904AAD bit 0.
void EventRoundFlag() {
    if ((B(::bof3::Img(0x904AA8)) & 0x40) && B(::bof3::Img(at::kTarget.v)) == 0) BossSe_Script |= 1;
}

// The action phase's test: actor 0, command kind 4, command id 0x78.
bool LeaderCommand78() {
    if (B(::bof3::Img(at::kActor.v)) != 0 || B(::bof3::Img(at::kCommandKind.v)) != 4) return false;
    return Word(At(BattleSprites_Action) + 2) == 0x78;
}

// Phase 3: the leader's HP 0 clears its +0x134 bit 1.
void EventLeaderDown() {
    if (ObjTrio[0].hp == 0) rv::Bytes(ObjTrio)[0x134] &= 0xFD;
}

// Phase 5: with 0x904AAD bit 0, the leader's command is kind 4, id 0x78, at
// target 0x40, and the battle's command kind and target the same.
void EventLeaderCommand() {
    if (!(BossSe_Script & 1)) return;
    SetWord(At(::bof3::Img(ObjTrio_addr) + 0x126), 0x78);
    rv::Bytes(ObjTrio)[0x125] = 4;
    B(::bof3::Img(at::kCommandKind.v)) = 4;
    rv::Bytes(ObjTrio)[0x124] = 0x40;
    B(::bof3::Img(at::kTarget.v)) = 0x40;
}

// The event hook of set-ups 4, 5 and 6 (three copies of one body).
unsigned char Kind3Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: EventRoundFlag(); break;
    case 1:
        if (LeaderCommand78()) {
            Battle_Enemies[0].hp = 1;
            Battle_Enemies[0].max_hp = 1;
            Battle_Enemies[1].hp = 1;
            Battle_Enemies[1].max_hp = 1;
            rv::Bytes(ObjTrio)[0x134] &= 0xFD;
        }
        break;
    case 3: EventLeaderDown(); break;
    case 5: EventLeaderCommand(); break;
    case 6: rv::Bytes(ObjTrio)[0x134] |= 2; break;
    default: break;   // 2, 4, and anything past 6 (the original's ja)
    }
    return 0;
}

// The end hook of set-ups 4 / 6 and 5: the loss (0x904AE8 bit 0) - the byte
// 0x92BF18 = `pick`, 0x904AE5 |= 0x80, 0x446E20, then the chapter's step =
// 0x32; otherwise the step up by one and 0x446E20.
void EndChapter(unsigned char pick) {
    if (Battle_EndFlags & 1) {
        BossH_LeaderPick = pick;
        EventBattle_Flags |= 0x80;
        BH_AT(Handler, at::kEndThird.v)();
        MoveScript_PartyRecords[0x65] = 0x32;
        return;
    }
    MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
    BH_AT(Handler, at::kEndThird.v)();
}

// BossActor_Find(tag) into Sprite_Current. The original writes through the
// answer untested; with no field actor carrying the tag it writes near
// address 0 and faults. Ours aborts there with a message (the owner's rule,
// round9 doc section 6; docs/boss_sb.md section 6), as BH's spawn writers do.
void FindActor(const char* who, unsigned tag) {
    unsigned char* const actor = BH_CALL(BossActor_Find)(tag);
    if (actor == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null answer (docs/boss_sb.md section 6)",
                    who, tag);
    Sprite_Current = actor;
}

// Field actor `tag` posed from an enemy: bit 0x40 cleared, the enemy's place
// copied, Sprite_Current = the actor, animation bank 0x5F, +0x48 = 0, the
// flip +0x2A, animation 1, and the enemy's words +0x58 / +0x5A (Sprite_Current
// read after each call).
void PoseActor(unsigned tag, U enemy, U pose, unsigned char flip) {
    BH_CALL(BossActor_ClearBit40)(tag);
    BH_CALL(BossActor_CopyFrom)(tag, At(enemy), 1);
    FindActor("PoseActor", tag);
    BH_CALL(Sprite_SetAnimationBank)(0x5F);
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x2A] = flip;
    BH_CALL(Sprite_SetAnimation)(1);
    SetWord(Sprite_Current + 0x58, Word(At(pose)));
    SetWord(Sprite_Current + 0x5A, Word(At(pose + 2)));
}

// The exit hook of set-ups 4, 5, 6: the loss clears bit 0x40 of the two
// actors; otherwise each is posed from its enemy (0 and 1).
void ExitPosePair(unsigned tag0, unsigned char flip) {
    if (Battle_EndFlags & 1) {
        BH_CALL(BossActor_ClearBit40)(tag0);
        BH_CALL(BossActor_ClearBit40)(tag0 + 1);
        return;
    }
    PoseActor(tag0, ::bof3::Img(Battle_Enemies_addr), ::bof3::Img(BossSb_Enemy0Pose_addr), flip);
    PoseActor(tag0 + 1, ::bof3::Img(Battle_Enemies_addr) + 0x128, ::bof3::Img(BossSb_Enemy1Pose_addr), flip);
}

}  // namespace

// original 0x438430: Boss_SetupTable[4]: End Boss04_End, Exit Boss04_Exit,
// Event Boss04_Event.
extern "C" void __cdecl Boss04_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss04_End), ::bof3::Code(bof3::addr::Boss04_Exit), ::bof3::Code(bof3::addr::Boss04_Event)); }

// original 0x438450: set-up 4's event hook: by the phase code's low byte, a
// jump table of 7 (past 6 nothing) - 0: with 0x904AA8 bit 0x40 and the
// target 0, 0x904AAD |= 1; 1: when actor 0 gives command kind 4 with id 0x78
// (the word +2 of [0x904B40]), enemies 0 and 1's words +0xA4 and +0xB0 = 1 and
// the leader's +0x134 bit 1 cleared; 3: the leader's HP 0 clears that bit; 5:
// with 0x904AAD bit 0, the leader's command (+0x124 target 0x40, +0x125 kind
// 4, +0x126 id 0x78) and the battle's kind and target the same; 6: the bit
// set. al 0.
extern "C" unsigned char __cdecl Boss04_Event(unsigned code) { return Kind3Event(code); }

// original 0x438560: set-up 4's exit hook: the loss (0x904AE8 bit 0) clears
// bit 0x40 of actors 0 and 1; otherwise actors 0 and 1 posed from enemies 0
// and 1 (flip 1).
extern "C" void __cdecl Boss04_Exit(void) { ExitPosePair(0, 1); }

// original 0x438650: Boss_SetupTable[5].
extern "C" void __cdecl Boss05_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss05_End), ::bof3::Code(bof3::addr::Boss05_Exit), ::bof3::Code(bof3::addr::Boss05_Event)); }

// original 0x438670: set-up 5's event hook, the body of Boss04_Event.
extern "C" unsigned char __cdecl Boss05_Event(unsigned code) { return Kind3Event(code); }

// original 0x438780: set-up 5's end hook: Boss04_End's with 0x92BF18 = 8.
extern "C" void __cdecl Boss05_End(void) { EndChapter(8); }

// original 0x4387C0: set-up 5's exit hook: actors 2 and 3 (flip 0).
extern "C" void __cdecl Boss05_Exit(void) { ExitPosePair(2, 0); }

// original 0x4388B0: Boss_SetupTable[6] (its end hook Boss04_End).
extern "C" void __cdecl Boss06_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss04_End), ::bof3::Code(bof3::addr::Boss06_Exit), ::bof3::Code(bof3::addr::Boss06_Event)); }

// original 0x4388D0: set-up 6's event hook, the body of Boss04_Event.
extern "C" unsigned char __cdecl Boss06_Event(unsigned code) { return Kind3Event(code); }

// original 0x4389E0: the end hook of set-ups 4 and 6: the loss - 0x92BF18 =
// 7, 0x904AE5 |= 0x80, 0x446E20, the chapter's step 0x8034E5 = 0x32;
// otherwise the step up by one and 0x446E20.
extern "C" void __cdecl Boss04_End(void) { EndChapter(7); }

// original 0x438A20: set-up 6's exit hook: actors 4 and 5 (flip 1); after
// actor 5 (not on the loss's path) its dwords +0x34 / +0x38 / +0x3C = enemy
// 1's.
extern "C" void __cdecl Boss06_Exit(void) {
    if (Battle_EndFlags & 1) {
        BH_CALL(BossActor_ClearBit40)(4);
        BH_CALL(BossActor_ClearBit40)(5);
        return;
    }
    PoseActor(4, ::bof3::Img(Battle_Enemies_addr), ::bof3::Img(BossSb_Enemy0Pose_addr), 1);
    PoseActor(5, ::bof3::Img(Battle_Enemies_addr) + 0x128, ::bof3::Img(BossSb_Enemy1Pose_addr), 1);
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(BossSb_Enemy1Place));
    SetLong(Sprite_Current + 0x38, Battle_Enemies[1].z);
    SetLong(Sprite_Current + 0x3C, Battle_Enemies[1].height);
}

void Boss04_Inject() {
    BOF3_INJECT(Boss04_Setup);
    BOF3_INJECT(Boss04_Event);
    BOF3_INJECT(Boss04_Exit);
    BOF3_INJECT(Boss05_Setup);
    BOF3_INJECT(Boss05_Event);
    BOF3_INJECT(Boss05_End);
    BOF3_INJECT(Boss05_Exit);
    BOF3_INJECT(Boss06_Setup);
    BOF3_INJECT(Boss06_Event);
    BOF3_INJECT(Boss04_End);
    BOF3_INJECT(Boss06_Exit);
}

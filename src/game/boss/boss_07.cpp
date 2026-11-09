// Moved here by tools/move_funcs.py from boss_sb.cpp (docs/source-layout-round.md).
#include "game/boss/boss_07.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb.h"
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

}  // namespace

// original 0x438C30: Boss_SetupTable[7].
extern "C" void __cdecl Boss07_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss07_End), ::bof3::Code(bof3::addr::Boss07_Exit), ::bof3::Code(bof3::addr::Boss07_Event)); }

// original 0x438C50: set-up 7's event hook, by the phase code's low byte (a
// jump table of 7; past 6 nothing): 0, 3, 5 and 6 as Boss04_Event's; 1: on
// actor 0's command kind 4 id 0x78, 0x904AE8 |= 4, the leader's +0x134 bit 1
// cleared, Str_CopyN(0x904EA0, 0x65D008, 0x10), BattleBanner_Add(1, 1, 0,
// 0x1E, 0x904EA0), 0x8031F3 = 1, Sprite_Current = the leader,
// Sprite_SetAnimation(0x2C), then the phase 0x904AA0 = 4, 0x904AA5 = 0x3C,
// the step 0x904AA1 = 2; 2: with 0x904AE8 bit 2, 0x904AA5 down by one - al
// 0xFF while it is not 0 (BattleRoundEnd_NextRound holds the round). al 0
// otherwise.
extern "C" unsigned char __cdecl Boss07_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: EventRoundFlag(); break;
    case 1:
        if (LeaderCommand78()) {
            Battle_EndFlags |= 4;
            rv::Bytes(ObjTrio)[0x134] &= 0xFD;
            char* const text = reinterpret_cast<char*>(At(::bof3::Img(at::kBannerText.v)));
            BH_CALL(Str_CopyN)(text, reinterpret_cast<const char*>(At(::bof3::Img(at::kBannerSource.v))), 0x10);
            BH_CALL(BattleBanner_Add)(1, 1, 0, 0x1E, text);
            WindowRecords[4].state = 1;
            Sprite_Current = At(::bof3::Img(ObjTrio_addr));
            BH_CALL(Sprite_SetAnimation)(0x2C);
            B(::bof3::Img(at::kPhase.v)) = 4;
            BossSb_Countdown = 0x3C;
            BattleActions_Step = 2;
        }
        break;
    case 2:
        if (Battle_EndFlags & 4) {
            const auto left = static_cast<unsigned char>(BossSb_Countdown - 1);
            BossSb_Countdown = left;
            if (left != 0) return 0xFF;
        }
        break;
    case 3: EventLeaderDown(); break;
    case 5: EventLeaderCommand(); break;
    case 6: rv::Bytes(ObjTrio)[0x134] |= 2; break;
    default: break;   // 4, and anything past 6
    }
    return 0;
}

// original 0x438DD0: set-up 7's end hook: the loss (0x904AE8 bit 0) - the
// chapter's step 0x32, 0x92BF18 = 6, the chapter's run 0x8034E4 = 6;
// otherwise the step up by one, 0x904AE8 |= 8, Draw_PassFlags 0, 0x92BF18 =
// 2, Music_Track 4. Then 0x446E20.
extern "C" void __cdecl Boss07_End(void) {
    if (Battle_EndFlags & 1) {
        MoveScript_PartyRecords[0x65] = 0x32;
        BossH_LeaderPick = 6;
        MoveScript_Var7 = 6;
    } else {
        const auto step = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
        const auto end = static_cast<unsigned char>(Battle_EndFlags | 8);
        Draw_PassFlags = 0;
        BossH_LeaderPick = 2;
        MoveScript_PartyRecords[0x65] = step;
        Battle_EndFlags = end;
        Music_Track = 4;
    }
    BH_AT(Handler, at::kEndThird.v)();
}

// original 0x438E30: the exit hook of set-ups 7 and 13: bit 0x40 of actors 0
// and 1 cleared.
extern "C" void __cdecl Boss07_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    BH_CALL(BossActor_ClearBit40)(1);
}

void Boss07_Inject() {
    BOF3_INJECT(Boss07_Setup);
    BOF3_INJECT(Boss07_Event);
    BOF3_INJECT(Boss07_End);
    BOF3_INJECT(Boss07_Exit);
}

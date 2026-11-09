// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_55.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_55_callees.h"
#include "game/boss_h.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sj.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_55::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The winning end hooks of set-ups 52 and 55: each party member whose +0 has
// bit 0 - Battle_RemoveFromTurnOrder(+5); then (read after the call) its +0x90
// and +8: Sprite_Current = the member, Sprite_PoseFromSet(+8 + 0x1C with
// +0x90 bit 14, else + 4 - a byte, 0x8C5D80, 0x1800).
void PoseParty() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(member_rec->direction + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(at::kPoseSet.v)), 0x1800);
    }
}

}  // namespace

// original 0x4406E0: Boss_SetupTable[55]. The hooks: end Boss55_End, exit
// BossHook_ExitActor0Bit40, event BareRetZero.
extern "C" void __cdecl Boss55_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::Boss55_End), ::bof3::Code(bof3::addr::BossHook_ExitActor0Bit40), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x440700: the end hook. Not won: a tail jump to 0x446E00. Won: the
// party posed (PoseParty), the chapter step 0x8034E5 = 0xF, 0x446E20 called
// (the end phase's step 3), then Music_Track = 0xFF.
extern "C" void __cdecl Boss55_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, boss_sj::at::kEndOther.v)();
        return;
    }
    PoseParty();
    MoveScript_PartyRecords[0x65] = 0xF;
    BH_AT(Handler, at::kEndThird.v)();
    Music_Track = 0xFF;
}

// original 0x440820: the exit hook of set-ups 3, 17, 55: BossActor_ClearBit40(0).
extern "C" void __cdecl BossHook_ExitActor0Bit40(void) { BH_CALL(BossActor_ClearBit40)(0); }

void Boss55_Inject() {
    BOF3_INJECT(Boss55_Setup);
    BOF3_INJECT(Boss55_End);
    BOF3_INJECT(BossHook_ExitActor0Bit40);
}

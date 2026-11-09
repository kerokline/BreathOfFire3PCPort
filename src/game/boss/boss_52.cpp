// Moved here by tools/move_funcs.py from boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_52.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sj::at;

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

// original 0x43F950: Boss_SetupTable[52]. The hooks: end Boss52_End, exit
// Boss52_Exit, event BareRetZero.
extern "C" void __cdecl Boss52_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::Boss52_End), ::bof3::Code(bof3::addr::Boss52_Exit), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43F970: the end hook. Not won (0x904AE8 bit 1 clear): a tail jump
// to 0x446E00 (the end phase, step 2). Won: the party posed (PoseParty), the
// chapter step 0x8034E5 = 0x1A, a tail jump to 0x446DE0 (step 1).
extern "C" void __cdecl Boss52_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, at::kEndOther.v)();
        return;
    }
    PoseParty();
    MoveScript_PartyRecords[0x65] = 0x1A;
    BH_AT(Handler, at::kEndWin.v)();
}

// original 0x43FA90: the exit hook. BossActor_ClearBit40(0); BossActor_Find(0)
// into Field_ActiveMember and Sprite_Current; Sprite_SetAnimation(5).
extern "C" void __cdecl Boss52_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(5);
}

void Boss52_Inject() {
    BOF3_INJECT(Boss52_Setup);
    BOF3_INJECT(Boss52_End);
    BOF3_INJECT(Boss52_Exit);
}

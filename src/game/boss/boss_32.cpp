// Moved here by tools/move_funcs.py from boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_32.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sg::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// The three set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSg(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The exit hooks of set-ups 31 and 32: BossActor_ClearBit40(0), the actor
// BossActor_Find(0) made Field_ActiveMember and Sprite_Current,
// Sprite_SetAnimation(animation).
void ActorZero(unsigned animation) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

}  // namespace

// original 0x43D250: Boss_SetupTable[32]. The hooks: end Boss32_End, exit
// Boss32_Exit, event BareRetZero.
extern "C" void __cdecl Boss32_Setup(void) {
    StoreHooksBossSg(::bof3::Code(bof3::addr::Boss32_End), ::bof3::Code(bof3::addr::Boss32_Exit), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43D270: the end hook. Won (0x904AE8 bit 1): for each party member
// 0..2 (ObjTrio) with +0 bit 0 - Battle_RemoveFromTurnOrder(its +5), its +0x90
// read after the call, Sprite_Current = the member, Sprite_PoseFromSet(its +8
// + 0x1C with +0x90 bit 14, else + 4 - a byte, 0x8C5D80, 0x1800); then
// 0x904AE8 read, Music_Track = 0x6B, the chapter's step = 0xE, 0x904AE8 = the
// value read | 8, a tail jump to 0x446DE0 (step 1). Otherwise a tail jump to
// 0x446E00 (step 2).
extern "C" void __cdecl Boss32_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, at::kEndOther.v)();
        return;
    }
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = L(p + 0x90);
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(member_rec->direction + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(at::kPoseSet.v)), 0x1800);
    }
    const unsigned char end = Battle_EndFlags;
    Music_Track = 0x6B;
    MoveScript_PartyRecords[0x65] = 0xE;
    Battle_EndFlags = static_cast<unsigned char>(end | 8);
    BH_AT(Handler, at::kEndWin.v)();
}

// original 0x43D3A0: the exit hook: ActorZero(0).
extern "C" void __cdecl Boss32_Exit(void) { ActorZero(0); }

void Boss32_Inject() {
    BOF3_INJECT(Boss32_Setup);
    BOF3_INJECT(Boss32_End);
    BOF3_INJECT(Boss32_Exit);
}

// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_38.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_si.h"
#include "game/boss_si_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_si::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void SetupBossSi(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The set-ups whose end is BH's BossHook_EndPickWay (fights 40, 42, 44, 45 -
// four copies of one body): exit BareRet, event BareRetZero.
void SetupPickWay() { SetupBossSi(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// The set-ups with an end of their own: exit BareRet, event BareRetZero.
void SetupOwnEnd(U end) { SetupBossSi(end, ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

}  // namespace

// original 0x43EE50: Boss_SetupTable[38]. End Boss38_End, exit BareRet, event
// BareRetZero; the target 0x904B44 = 0x40 (the enemies' side); Sprite_Current
// = enemy 7's object; MagicFx_CenterOnSide (Sprite_Current to the side's
// centre); then, Sprite_Current read after the call, MoveScript_F3Divisor =
// 0x20, Field_Kind2X = the centre's x + (its +0x34 - the centre's x) / 2 and
// Field_Kind2Z = the centre's z + (its +0x38 - the centre's z) / 2 (signed
// 32-bit differences, halved toward zero): the field's kind-2 point midway
// between the fight's centre and the enemies'. Sprite_Current is left at
// enemy 7's object.
extern "C" void __cdecl Boss38_Setup(void) {
    SetupOwnEnd(::bof3::Code(bof3::addr::Boss38_End));
    B(::bof3::Img(at::kTarget.v)) = 0x40;
    Sprite_Current = At(::bof3::Img(Battle_Enemies_addr) + 0x818);
    BH_CALL(MagicFx_CenterOnSide)();
    const unsigned char* const s = Sprite_Current;
    const U cx = BossSi_CentreX;
    MoveScript_F3Divisor = 0x20;
    Field_Kind2X = S(cx + static_cast<U>(S(static_cast<U>(Long(s + 0x34)) - cx) / 2));
    const U cz = BossSi_CentreZ;
    Field_Kind2Z = S(cz + static_cast<U>(S(static_cast<U>(Long(s + 0x38)) - cz) / 2));
}

// original 0x43EED0: the end hook: won, the movement script's variable 3 =
// 5, the chapter's step 0x20, Music_Track = 0x77, step 1; otherwise step 2.
extern "C" void __cdecl Boss38_End(void) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kScriptVar3.v)) = 5;
        MoveScript_PartyRecords[0x65] = 0x20;
        Music_Track = 0x77;
        BH_AT(Handler, at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, at::kEndOther.v)();
}

// original 0x43EF00: Boss_SetupTable[44]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss44_Setup(void) { SetupPickWay(); }

void Boss38_Inject() {
    BOF3_INJECT(Boss38_Setup);
    BOF3_INJECT(Boss38_End);
    BOF3_INJECT(Boss44_Setup);
}

// Moved here by tools/move_funcs.py from area_w2e.cpp (docs/source-layout-round.md).
#include "game/area/area_106.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char* Cur() { return Sprite_Current; }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char& B(U address) { return *At(address); }

constexpr ::bof3::image::CodeVa kArmTail36{bof3::addr::Area106_ArmTail36};

using ValueFn = void (__cdecl*)(U);

void Set40() { AH_CALL(ScriptFlags_Set40)(); }

}  // namespace

// original 0x416440 (area 106's handler 0, its descriptor 0x61C670's +0x3C
// 0x61C664; PSX 0x801F38E8): Field_ActiveMember +0x80 bit 0 cleared; when the
// byte of Character_IndexById 0x66972C by the leader's +0x89 is 0,
// Cond_ByteFE = 1 and Sprite_Current (read before) +0 = 0.
extern "C" void __cdecl Area106_Handler0(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(at::kEffectStates.v) + ObjTrio[0].char_id) != 0) return;
    unsigned char* const c = Cur();
    Cond_ByteFE = 1;
    c[0] = 0;
}

// original 0x416480 (area 106's handler 1; PSX 0x801F3940): places the running
// object by story flags 0x51 and 0x4F and moves the script on.
//   0x51 set: (0x128000, 0xB8000); 0x4F set - the word +0x3E 0x850 and the
//     script word MoveScript_Object +0xA + 2; clear - 0x600 and + 4.
//   0x51 clear: 0x4F set - (0x128000, 0x238000), 0x850, the script not moved;
//     clear - (0x118000, 0x1F8000), 0x300, + 4.
// (Sprite_Current read again for each store.)
extern "C" void __cdecl Area106_PlaceByFlags(void) {
    if (AH_CALL(Flags_Test)(Bank(), 0x51) != 0) {
        rv::Current<records::SpriteObject>()->x = 0x128000;
        rv::Current<records::SpriteObject>()->z = 0xB8000;
        if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x850);
            move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 2u);
        } else {
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x600);
            move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 4u);
        }
        return;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        rv::Current<records::SpriteObject>()->x = 0x128000;
        rv::Current<records::SpriteObject>()->z = 0x238000;
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x850);
        return;
    }
    rv::Current<records::SpriteObject>()->x = 0x118000;
    rv::Current<records::SpriteObject>()->z = 0x1F8000;
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x300);
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 4u);
}

// original 0x416550 (area 106's handler 2; PSX 0x801F3A60): k = (story flag
// 0x52 set) | (flag 0x53 set) << 1; the running object to the cell pair k of
// Area106_Cells 0x61C6B4 ((x << 16) | 0x8000, the same in z); then flag 0x4F
// set - the word +0x3E 0x850 and the script word + 8; clear - 0x280 and the
// script word + 2k.
extern "C" void __cdecl Area106_PlaceByPair(void) {
    const unsigned b52 = AH_CALL(Flags_Test)(Bank(), 0x52) != 0 ? 1u : 0u;
    const unsigned b53 = AH_CALL(Flags_Test)(Bank(), 0x53) != 0 ? 2u : 0u;
    const unsigned k = b52 | b53;
    const unsigned char* const pair = At(::bof3::Img(Area106_Cells_addr) + k * 2);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(static_cast<U>(pair[0]) << 16 | 0x8000u));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(static_cast<U>(pair[1]) << 16 | 0x8000u));
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x850);
        move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 8u);
        return;
    }
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x280);
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + k * 2u);
}

// original 0x416600 (Field_ModeTailKinds entry 36, armed by
// Area106_ArmTail36): by the s8 0x9039F4 (a byte table of 0x11 then a jump
// table of seven), each case Party_DropIn(n), 0x9039F3 = 0x9039F4 = 0, then
// the story flags: 0 - DropIn(0), set 0x51; 2 - DropIn(1), clear 0x51; 0xA -
// DropIn(2), set 0x52, clear 0x53; 0xC - DropIn(3), clear 0x52, set 0x53; 0xE
// - DropIn(4), set both; 0x10 - DropIn(5), clear both. Any other state
// (negative, odd, above 0x10, 4..8) nothing.
extern "C" void __cdecl Area106_Tail36(void) {
    const auto state = AreaW0c_TailState;
    unsigned entry;
    switch (state) {
    case 0x0: entry = 0; break;
    case 0x2: entry = 1; break;
    case 0xA: entry = 2; break;
    case 0xC: entry = 3; break;
    case 0xE: entry = 4; break;
    case 0x10: entry = 5; break;
    default: return;
    }
    AH_CALL(Party_DropIn)(entry);
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
    switch (entry) {
    case 0: AH_CALL(Flags_Set)(Bank(), 0x51); break;
    case 1: AH_CALL(Flags_Clear)(Bank(), 0x51); break;
    case 2: AH_CALL(Flags_Set)(Bank(), 0x52); AH_CALL(Flags_Clear)(Bank(), 0x53); break;
    case 3: AH_CALL(Flags_Clear)(Bank(), 0x52); AH_CALL(Flags_Set)(Bank(), 0x53); break;
    case 4: AH_CALL(Flags_Set)(Bank(), 0x52); AH_CALL(Flags_Set)(Bank(), 0x53); break;
    default: AH_CALL(Flags_Clear)(Bank(), 0x52); AH_CALL(Flags_Clear)(Bank(), 0x53); break;
    }
}

// original 0x416770 (area 106's step hook, through Area_StepHook at 0x56E132;
// (x, z) 16.16): al 0 when Cond_ByteFD is set. Otherwise one cell arms tail
// kind 36 (Area106_ArmTail36) with the state, al 1:
//   flag 0x4F set: 0x51 clear - z exactly 0x248000 and x's high word 0x11..0x13
//     (u16): state 0; 0x51 set - z 0xA8000, x's high word 0x12..0x13: 2.
//   0x4F clear, 0x52 set: z 0x258000, x's high word 0x1A..0x1B: 0x10 with 0x53
//     set, else 0xC.
//   0x4F and 0x52 clear, 0x53 set: x exactly 0x268000, z's high word
//     0x24..0x25: 0xE; 0x53 clear: x 0x188000, z's high word 0x15..0x16: 0xA.
// Any other cell al 0.
extern "C" unsigned char __cdecl Area106_StepHook(U x, U z) {
    if (Cond_ByteFD != 0) return 0;
    const auto arm = AH_AT(ValueFn, kArmTail36.v);
    const auto in = [](U word, U lo, U n) { return static_cast<std::uint16_t>((word >> 16) - lo) < n; };
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        if (AH_CALL(Flags_Test)(Bank(), 0x51) == 0) {
            if (z != 0x248000 || !in(x, 0x11, 3)) return 0;
            arm(0);
            return 1;
        }
        if (z != 0xA8000 || !in(x, 0x12, 2)) return 0;
        arm(2);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x52) != 0) {
        if (z != 0x258000 || !in(x, 0x1A, 2)) return 0;
        arm(AH_CALL(Flags_Test)(Bank(), 0x53) != 0 ? 0x10 : 0xC);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x53) != 0) {
        if (x != 0x268000 || !in(z, 0x24, 2)) return 0;
        arm(0xE);
        return 1;
    }
    if (x != 0x188000 || !in(z, 0x15, 2)) return 0;
    arm(0xA);
    return 1;
}

// original 0x4168C0 (state) (called by Area106_StepHook): ScriptFlags_Set40;
// mode-tail kind 36 armed with the state's low byte.
extern "C" void __cdecl Area106_ArmTail36(U state) {
    Set40();
    AreaW0a_TailKind = 0x24;
    AreaW0c_TailState = static_cast<unsigned char>(state);
}

void Area106_Inject() {
    BOF3_INJECT(Area106_Handler0);
    BOF3_INJECT(Area106_PlaceByFlags);
    BOF3_INJECT(Area106_PlaceByPair);
    BOF3_INJECT(Area106_Tail36);
    BOF3_INJECT(Area106_StepHook);
    BOF3_INJECT(Area106_ArmTail36);
}

// Moved here by tools/move_funcs.py from area_w4c.cpp (docs/source-layout-round.md).
#include "game/area/area_173.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

U Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

// The movement script back (or on) by `delta` words: the running script
// object's u16 +0xA, the pointer read at the store.
void ScriptStep(unsigned delta) { AddWord(MoveScript_Object + 0xA, delta); }

// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). The handlers write
// through Effect_FindFree's slot, or one an earlier handler kept in the
// running object's +0xB, unchecked: past the 20 records the original writes
// into what follows them - ours aborts.
unsigned char* EffectAt(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride));
    return Mem(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
}

// Field_ActiveMember's index among the field objects as the originals compute
// it: (the pointer - Sprite_Objects) / 0xA4, C's truncating signed division
// (imul 0x63E7063F, sar 6, plus the sign bit); only its low byte is kept.
unsigned char ActiveMemberIndex() {
    const auto diff = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(diff / static_cast<std::int32_t>(at::kObjectStride));
}

// Areas 173 and 174's effect of kind 0x9D (handlers 5 and 6 of 173, 3 and 4
// of 174): +0 = 1, kind +5 = 0x9D, +6 = `sub`, +0xB = Field_ActiveMember's
// index byte, word +0x2E = `word2E`, word +0x30 = 0xA5.
void Effect9D(const char* who, unsigned slot, unsigned char sub, unsigned char index, unsigned word2E) {
    unsigned char* const e = EffectAt(who, slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x9D;
    e[6] = sub;
    e[0xB] = index;
    tE->screen_x = word2E;
    tE->screen_y = 0xA5;
}

// Area 173's handlers 1 and 2: over the four (member byte, message) pairs, the
// first member byte some party member below Field_MemberCount has at +0x89
// opens its message: Msg_OpenScript(the word), Field_Request = 2. The member
// loop is unsigned and unchecked (a count past 3 reads the records after the
// party's, as the original does).
void MessageByMember(U members, U messages) {
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < at::kArea173ListCount; ++i) {
        if (count == 0) continue;
        const unsigned char id = B(members + i);
        for (unsigned m = 0; m < count; ++m) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + m * at::kPartyStride) != id) continue;
            AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(messages + i * 2))));
            Field_Request = 2;
            return;
        }
    }
}

}  // namespace

// original 0x428450 (Area173_Handlers[1]; PSX 0x801F48C4): the first of
// Area173_MembersA a party member has at +0x89 opens its message from
// Area173_MessagesA; Field_Request = 2.
extern "C" void __cdecl Area173_MessageByMemberA(void) { MessageByMember(::bof3::Img(Area173_MembersA_addr), ::bof3::Img(Area173_MessagesA_addr)); }

// original 0x4284E0 (Area173_Handlers[2]; PSX 0x801F4988): the same over
// Area173_MembersB / Area173_MessagesB.
extern "C" void __cdecl Area173_MessageByMemberB(void) { MessageByMember(::bof3::Img(Area173_MembersB_addr), ::bof3::Img(Area173_MessagesB_addr)); }

// original 0x428570 (Area173_Handlers[3]; PSX 0x801F4A4C): the running object
// at x 0x168000, y 0x5000000 and z 0x7A8000 when Field_State's +0x89 is 2,
// else 0x7A0000; its direction +8 = 1.
extern "C" void __cdecl Area173_PlaceObject(void) {
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, 0x168000);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, 0x5000000);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Field_State[0x89] == 2 ? 0x7A8000 : 0x7A0000);
    Sprite_Current[8] = 1;
}

// original 0x4285D0 (Area173_Handlers[4]; PSX 0x801F4A9C; also area 128's
// choice 3 and handler 1 - one body, in area 173's block): Field_State's +0x89
// at 2: the script on 1.
extern "C" void __cdecl Area173_ScriptOnIfMember2(void) {
    if (Field_State[0x89] == 2) ScriptStep(1);
}

// original 0x4285F0 (Area173_Handlers[5]; PSX 0x801F4ADC): Effect_FindFree; a
// slot: kind 0x9D, +6 = 0, +0xB Field_ActiveMember's index, word +0x2E = 0xC,
// +0x30 = 0xA5.
extern "C" void __cdecl Area173_Effect9DSub0(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    Effect9D("Area173_Effect9DSub0", slot, 0, ActiveMemberIndex(), 0xC);
}

// original 0x428660 (Area173_Handlers[6]; PSX 0x801F4BB0): the same with +6 =
// 1.
extern "C" void __cdecl Area173_Effect9DSub1(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    Effect9D("Area173_Effect9DSub1", slot, 1, ActiveMemberIndex(), 0xC);
}

// original 0x4286D0 (Field_ModeTailKinds[38]): by the s8 tail state through
// the byte table 0x4287D0 (13 entries) and the jump table 0x4287B8 (6) in its
// own extent; a state below 0 or above 12, and states 2..9, return at once.
//   0 (the arrive hook): Party_DropIn(0), state 1.
//   1: counter 3 at 0x18: Field_ChangeArea(0xBA, 0x48000, zone 2 (Cond_ByteFD)
//      ? 0xA0000 : 0x640000, 0x80), story flag 0x58 set, disarmed.
//   10 (the init, flag 0x58 set): counter 3 at 0x20: Cond_ByteFE = 1, the
//      word timer 0x1E, state 11.
//   11: the word timer less 1; at 0: counter 3 = 0x24, state 12.
//   12: counter 3 at 0: story flag 0x58 cleared, disarmed.
extern "C" void __cdecl Area173_Tail38(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0:
        AH_CALL(Party_DropIn)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x18) return;
        AH_CALL(Field_ChangeArea)(0xBA, 0x48000, Cond_ByteFD == 2 ? 0xA0000 : 0x640000, 0x80);
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x58);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 10:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x20) return;
        Cond_ByteFE = 1;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 11: {
        const unsigned timer = (Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1u) & 0xFFFF;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), timer);
        if (timer != 0) return;
        B(::bof3::Img(at::kCounter3.v)) = 0x24;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        return;
    }
    case 12:
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x58);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    default: return;
    }
}

// original 0x4287E0 (Area_ArriveHook's case for area 173, a call at
// 0x56E5A9): (x, z). Zone 1 (Cond_ByteFD) with z exactly 0x460000 and x's high
// word 0x11..0x13, or zone 2 with z 0x600000 and x's high word 0x30..0x32 (a
// 16-bit unsigned test): ScriptFlags_Set40, tail kind 0x26 at state 0, al 1.
// Else al 0.
extern "C" int __cdecl Area173_ArriveHook(unsigned x, unsigned z) {
    const unsigned char zone = Cond_ByteFD;
    const auto high = static_cast<unsigned short>(x >> 16);
    if (zone == 1) {
        if (z != 0x460000) return 0;
        if (static_cast<unsigned short>(high - 0x11) >= 3) return 0;
    } else if (zone == 2) {
        if (z != 0x600000) return 0;
        if (static_cast<unsigned short>(high - 0x30) >= 3) return 0;
    } else {
        return 0;
    }
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x26;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    return 1;
}

// original 0x428840 (Area173's +0x40 init; PSX 0x801F4E90): story flag 0x58
// set: tail kind 0x26 at state 0xA.
extern "C" void __cdecl Area173_Init(void) {
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x58) == 0) return;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x26;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
}

void Area173_Inject() {
    BOF3_INJECT(Area173_MessageByMemberA);
    BOF3_INJECT(Area173_MessageByMemberB);
    BOF3_INJECT(Area173_PlaceObject);
    BOF3_INJECT(Area173_ScriptOnIfMember2);
    BOF3_INJECT(Area173_Effect9DSub0);
    BOF3_INJECT(Area173_Effect9DSub1);
    BOF3_INJECT(Area173_Tail38);
    BOF3_INJECT(Area173_ArriveHook);
    BOF3_INJECT(Area173_Init);
}

// Moved here by tools/move_funcs.py from area_w4b.cpp (docs/source-layout-round.md).
#include "game/area/area_170.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4b::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

// A word in [lo, lo + 3), compared as the originals' `sub; cmp r16, 3; jb`.
bool In3(std::uint16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 3; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* Story() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The tail armed: ScriptFlags_Set40, then kind and state.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The tail ended: kind and state 0.
void EndTail() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    EndTail();
}

// A choice that arms a tail on answer 0 (areas 168, 170): the answer read,
// the message 0xFFFF, then for 0 ScriptFlags_Set40 and the tail.
void ChoiceArm(unsigned char kind, unsigned char state) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail(kind, state);
}

// Area 170's tail: on counter 3 at `count`, story flag `flag` and
// Field_ChangeArea(0xAA, x, z, flags), state 30.
void ChangeIfCounter3(unsigned char count, unsigned flag, int x, int z, unsigned flags) {
    if (B(::bof3::Img(at::kCounter3.v)) != count) return;
    AH_CALL(Flags_Set)(Story(), flag);
    AH_CALL(Field_ChangeArea)(0xAA, x, z, flags);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1E;
}

// Area 170's tail: Party_DropIn(entry), then `next`.
void DropIn(unsigned entry, unsigned char next) {
    AH_CALL(Party_DropIn)(entry);
    B(::bof3::Img(AreaW0c_TailState_addr)) = next;
}

// Area 170's tail states 41 and 44: an effect of kind 0x13 at a free slot
// (kept in the scratch byte, read back): live, its +0x64, +0x68 (the camera
// yaw, sign-extended, read after the search), +0x6C, and +9.
void SpawnEffect13(std::int32_t a, std::int32_t c, unsigned char nine) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(::bof3::Img(at::kScratch.v)) = slot;
    if (slot == 0xFF) return;
    const std::int32_t yaw = static_cast<std::int16_t>(Word(Mem(::bof3::Img(Camera_Angles_addr) + 0x2)));
    unsigned char* const e = EffectAt(B(::bof3::Img(at::kScratch.v)));
    e[0] = 1;
    e[5] = 0x13;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[0], a);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[1], yaw);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[2], c);
    e[9] = nine;
}

// MoveScript_F3Divisor = Field_MoveSpeeds byte << 3, 16 bits.
void SetDivisor(U speed) { SetWord(Mem(::bof3::Img(MoveScript_F3Divisor_addr)), static_cast<unsigned>(B(speed)) << 3); }

}  // namespace

// original 0x426B60 (area 170 +0x34[0]): answer 0 arms tail kind 37 at 5.
extern "C" void __cdecl Area170_ChoiceArm37At5(void) { ChoiceArm(0x25, 5); }

// original 0x426B90 (area 170 +0x34[1]): answer 0 arms tail kind 37 at 15.
extern "C" void __cdecl Area170_ChoiceArm37At15(void) { ChoiceArm(0x25, 0xF); }

// original 0x426BC0 (area 170 +0x34[2]): answer 0 arms tail kind 37 at 25.
extern "C" void __cdecl Area170_ChoiceArm37At25(void) { ChoiceArm(0x25, 0x19); }

// original 0x426BF0 (area 170 +0x34[3]): message 0xFFFF; the tail's state 52
// for answer 0, 55 for any other (the kind is not written).
extern "C" void __cdecl Area170_ChoiceState52Or55(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(::bof3::Img(AreaW0c_TailState_addr)) = answer != 0 ? 0x37 : 0x34;
}

// original 0x426C10 (area 170 +0x3C[0] = +0x34[4]; PSX 0x801F4A7C):
// Field_ScriptFlags |= 0x1010.
extern "C" void __cdecl Area170_ScriptFlagsSet1010(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1010); }

// original 0x426C20 (area 170 +0x3C[1] = +0x34[5]; PSX 0x801F4A9C):
// Field_ScriptFlags &= 0xEFEF.
extern "C" void __cdecl Area170_ScriptFlagsClear1010(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF); }

// original 0x426C30 (area 170 +0x40; PSX 0x801F4ABC): tail kind 51 becomes
// 37; the engine's 0x486D60 (EffectKind7D_SetMap); then the entry zone 2 with Cond_ByteFD 1 plays
// sound 0x20C (zone 2 with another value returns), and the zone (read again)
// 3 with Cond_ByteFD 2 plays it.
extern "C" void __cdecl Area170_Init(void) {
    if (B(::bof3::Img(AreaW0a_TailKind_addr)) == 0x33) B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x25;
    AH_AT(Handler, at::kArea170MapSetUp.v)();
    if (B(::bof3::Img(AreaW1a_EntryZone_addr)) == 2) {
        if (Cond_ByteFD != 1) return;
        AH_CALL(Sound_PlayEffect)(0x20C);
    }
    if (B(::bof3::Img(AreaW1a_EntryZone_addr)) == 3 && Cond_ByteFD == 2) AH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x426C90 (tail kind 37): 61 states through a byte table into 25
// cases (docs/area_w4b.md section 1). A state outside 0..60 or on no case:
// nothing.
extern "C" void __cdecl Area170_Tail37(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0: DropIn(0, 1); return;
    case 1: ChangeIfCounter3(0x24, 0x55, 0x60000, 0x3F0000, 0x82); return;
    case 5: DropIn(1, 6); return;
    case 6: ChangeIfCounter3(0x24, 0x55, 0x40000, 0x750000, 0x83); return;
    case 10: DropIn(4, 0xB); return;
    case 11: ChangeIfCounter3(0x44, 0x56, 0x100000, 0x5D0000, 0x86); return;
    case 15: DropIn(5, 0x10); return;
    case 16: ChangeIfCounter3(0x44, 0x56, 0xA0000, 0x930000, 0x87); return;
    case 20: DropIn(8, 0x15); return;
    case 21: ChangeIfCounter3(0x64, 0x57, 0x170000, 0x5D0000, 0x8A); return;
    case 25: DropIn(9, 0x1A); return;
    case 26: ChangeIfCounter3(0x64, 0x57, 0x110000, 0x930000, 0x8B); return;
    case 29:
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1010);
        AH_CALL(Flags_Clear)(Story(), 0x57);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x35;
        return;
    case 30:
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        AH_CALL(Flags_Clear)(Story(), 0x55);
        AH_CALL(Flags_Clear)(Story(), 0x56);
        AH_CALL(Flags_Clear)(Story(), 0x57);
        EndTail();
        return;
    case 40: {
        // an effect of kind 0x7D: +0x2E and +0x30 words 0x64, +6 the message
        // word's low byte - 0xB (read after the search); the timer 0
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            const auto six = static_cast<unsigned char>(B(::bof3::Img(at::kMessage.v)) - 0xB);
            unsigned char* const e = EffectAt(slot);
            auto* const tE = rv::As<records::EffectObject>(e);
            e[0] = 1;
            e[5] = 0x7D;
            tE->screen_x = 0x64;
            tE->screen_y = 0x64;
            e[6] = six;
        }
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x29;
        return;
    }
    case 41:
        // the timer 1: the glide divisor from speed 2, Field_Kind2X / Z a fixed
        // point, an effect of kind 0x13, state 42; then (either way) the timer
        // 0xFF: state 45
        if (Word(Mem(::bof3::Img(at::kTailTimer.v))) == 1) {
            SetDivisor(::bof3::Img(Field_MoveSpeeds_addr) + 0x2);
            SetLong(Mem(::bof3::Img(Field_Kind2X_addr)), 0x1A0000);
            SetLong(Mem(::bof3::Img(Field_Kind2Z_addr)), 0x910000);
            SpawnEffect13(0, 0, 0x70);
            B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2A;
        }
        if (Word(Mem(::bof3::Img(at::kTailTimer.v))) == 0xFF) B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2D;
        return;
    case 42:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2B;
        return;
    case 43:
        AddWord(Mem(::bof3::Img(at::kTailTimer.v)), -1);
        if (Word(Mem(::bof3::Img(at::kTailTimer.v))) != 0) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2C;
        return;
    case 44: {
        AddWord(Mem(::bof3::Img(at::kTailTimer.v)), -1);
        if (Word(Mem(::bof3::Img(at::kTailTimer.v))) != 0) return;
        // the glide divisor from speed 3, Field_Kind2X / Z the leader's
        // position, an effect of kind 0x13, state 45
        const unsigned speed = B(::bof3::Img(Field_MoveSpeeds_addr) + 0x3);
        const std::int32_t x = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
        const std::int32_t z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
        SetWord(Mem(::bof3::Img(MoveScript_F3Divisor_addr)), speed << 3);
        SetLong(Mem(::bof3::Img(Field_Kind2X_addr)), x);
        SetLong(Mem(::bof3::Img(Field_Kind2Z_addr)), z);
        SpawnEffect13(static_cast<std::int32_t>(0xFFFFFD56u), 0x200, 0x38);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2D;
        return;
    }
    case 45:
        if (Field_Kind2Hold != 0) return;
        Disarm();
        return;
    case 50:
        AH_CALL(Msg_OpenScript)(0xA);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x33;
        return;
    case 52:
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0xC);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x35;
        return;
    case 53: {
        // while Field_Request is 0: the held word kept in the scratch cells,
        // its top nibble swapped through Area170_InputSwap
        if (Field_Request != 0) return;
        const std::uint16_t held = Word(Mem(::bof3::Img(Input_Held_addr)));
        SetWord(Mem(::bof3::Img(at::kScratch.v)), held);
        const unsigned swapped = static_cast<unsigned>(B(::bof3::Img(Area170_InputSwap_addr) + (held >> 12))) << 12;
        SetWord(Mem(::bof3::Img(Input_Held_addr)), (swapped | (held & 0xFFFu)) & 0xFFFFu);
        return;
    }
    case 55:
        if (Field_Request == 2) return;
        Disarm();
        return;
    case 60:
        AH_CALL(ScriptFlags_Clear40)();
        AddWord(Mem(::bof3::Img(ObjTrio_addr) + 0x27a), 1);
        AddWord(Mem(::bof3::Img(ObjTrio_addr) + 0x3c6), 1);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF);
        EndTail();
        return;
    default: return;
    }
}

// original 0x427270 (Area_StepHook's case for area 170, 0x56E1C2): by
// Cond_ByteFD - 4: z exactly 0x708000 or 0x738000 with x's high word 3..5, or
// x exactly 0x28000 or 0x58000 with z's high word 0x71..0x73: tail kind 37
// armed at 0, al 1. 5: z 0x8E8000 / 0x918000 with x's high word 9..11, or x
// 0x88000 / 0xB8000 with z's 0x8F..0x91: armed at 10, al 1; z 0x8E8000 /
// 0x918000 with x's 0x10..0x12, or x 0xF8000 / 0x128000 with z's 0x8F..0x91:
// armed at 20, story flag 0x7E set, Field_ScriptFlags &= 0xEFEF, al 1; else,
// story flag 0x7E clear, z exactly 0x900000 and x exactly 0x208000 with the
// leader's pose 7: armed at 50, al 1; x exactly 0x218000 with pose 3: kind 37
// and state 60 written without ScriptFlags_Set40, al 0. Anything else al 0.
extern "C" unsigned char __cdecl Area170_StepHook(long x, long z) {
    const auto ux = static_cast<U>(x);
    const auto uz = static_cast<U>(z);
    const unsigned char fd = Cond_ByteFD;
    if (fd == 4) {
        if ((uz == 0x708000 || uz == 0x738000) && In3(High(x), 3)) {
            ArmTail(0x25, 0);
            return 1;
        }
        if ((ux == 0x28000 || ux == 0x58000) && In3(High(z), 0x71)) {
            ArmTail(0x25, 0);
            return 1;
        }
        return 0;
    }
    if (fd != 5) return 0;
    if (((uz == 0x8E8000 || uz == 0x918000) && In3(High(x), 9)) || ((ux == 0x88000 || ux == 0xB8000) && In3(High(z), 0x8F))) {
        ArmTail(0x25, 0xA);
        return 1;
    }
    if (((uz == 0x8E8000 || uz == 0x918000) && In3(High(x), 0x10)) || ((ux == 0xF8000 || ux == 0x128000) && In3(High(z), 0x8F))) {
        ArmTail(0x25, 0x14);
        AH_CALL(Flags_Set)(Story(), 0x7E);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Story(), 0x7E) != 0) return 0;
    if (ux == 0x208000) {
        if (uz != 0x900000 || B(::bof3::Img(ObjTrio_addr) + 0x8) != 7) return 0;
        ArmTail(0x25, 0x32);
        return 1;
    }
    if (ux == 0x218000 && uz == 0x900000 && B(::bof3::Img(ObjTrio_addr) + 0x8) == 3) {
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x25;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x3C;
    }
    return 0;
}

// original 0x427470 (Field_ObjectTriggers id 19; a gap of the tool): tail
// kind 37 armed at 40, story flag 0x7E cleared, al 1.
extern "C" unsigned char __cdecl Area170_Trigger19(unsigned char*, unsigned char*) {
    ArmTail(0x25, 0x28);
    AH_CALL(Flags_Clear)(Story(), 0x7E);
    return 1;
}

// original 0x4274A0 (Field_ObjectTriggers id 20; a gap): story flag 0x67, al 0.
extern "C" unsigned char __cdecl Area170_Trigger20(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(Story(), 0x67);
    return 0;
}

// original 0x4274C0 (Field_ObjectTriggers id 56; a gap): Sprite_Current kept;
// story flag 0x79 clear: Inventory_Add(1, 0x4B, 1) (the original pushes a
// fourth word, 0, the callee does not read) - added: flag 0x79 set,
// Sprite_SetAnimation(1), message 0x22, sound 0x106; not added: system
// message 3. Flag 0x79 set: system message 1. Then Party_DropIn(0xD),
// Sprite_Current put back, Field_Request 2, al 0.
extern "C" unsigned char __cdecl Area170_Trigger56(unsigned char*, unsigned char*) {
    unsigned char* const saved = Sprite_Current;
    if (AH_CALL(Flags_Test)(Story(), 0x79) == 0) {
        if (AH_CALL(Inventory_Add)(1, 0x4B, 1) != 0) {
            AH_CALL(Flags_Set)(Story(), 0x79);
            AH_CALL(Sprite_SetAnimation)(1);
            AH_CALL(Msg_OpenScript)(0x22);
            AH_CALL(Sound_PlayEffect)(0x106);
        } else {
            AH_CALL(Msg_OpenSystem)(3);
        }
    } else {
        AH_CALL(Msg_OpenSystem)(1);
    }
    AH_CALL(Party_DropIn)(0xD);
    Sprite_Current = saved;
    Field_Request = 2;
    return 0;
}

void Area170_Inject() {
    BOF3_INJECT(Area170_ChoiceArm37At5);
    BOF3_INJECT(Area170_ChoiceArm37At15);
    BOF3_INJECT(Area170_ChoiceArm37At25);
    BOF3_INJECT(Area170_ChoiceState52Or55);
    BOF3_INJECT(Area170_ScriptFlagsSet1010);
    BOF3_INJECT(Area170_ScriptFlagsClear1010);
    BOF3_INJECT(Area170_Init);
    BOF3_INJECT(Area170_Tail37);
    BOF3_INJECT(Area170_StepHook);
    BOF3_INJECT(Area170_Trigger19);
    BOF3_INJECT(Area170_Trigger20);
    BOF3_INJECT(Area170_Trigger56);
}

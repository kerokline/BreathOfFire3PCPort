// Moved here by tools/move_funcs.py from area_w3e.cpp (docs/source-layout-round.md).
#include "game/area/area_140.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`) and Sprite_Objects record `slot` (0xA4, likewise).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data or the next table).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed with ScriptFlags_Set40 first: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The first of `count` entries of `stride` bytes at `table` whose x byte is
// x's low byte, whose z byte is z's, and whose direction (the third byte's low
// nibble) is the leader's direction byte (the whole byte: 0x10 or more matches
// none); `count` when none - the search the three cell hooks open with.
unsigned FindCell(U table, unsigned count, unsigned stride, long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const e = Mem(table + i * stride);
        if (e[0] == static_cast<unsigned char>(x) && e[1] == static_cast<unsigned char>(z) && (e[2] & 0xF) == pose) return i;
    }
    return count;
}

}  // namespace

// original 0x41F5C0 (area 140 +0x34[0] = +0x3C[2]; PSX 0x801F3D4C): answer 0:
// message 4; answer 1: ScriptFlags_Set40, tail kind 41 at state 0, message
// 0xFFFF; any other: message 0xFFFF.
extern "C" void __cdecl Area140_ChoiceArmTail41(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    if (answer == 0) {
        SetMessage(4);
        return;
    }
    if (answer == 1) ArmTail(0x29, 0);
    SetMessage(0xFFFF);
}

// original 0x41F600 (area 140 +0x3C[0]; PSX 0x801F3DAC): the active member's
// +0x80 bit 0 cleared; the leader's +0x89 at 5, 6 or 7: story flag 0x54,
// MoveCmd_TestFB(2, 7), Sprite_Current's +0 = 0.
extern "C" void __cdecl Area140_FlagIfLeader89Is567(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    const unsigned char v = B(::bof3::Img(ObjTrio_addr) + 0x89);
    if (v != 5 && v != 6 && v != 7) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x54);
    AH_CALL(MoveCmd_TestFB)(2, 7);
    Sprite_Current[0] = 0;
}

// original 0x41F650 (area 140 +0x3C[1]; PSX 0x801F3E30): story flag 0x66
// clear: the script position + 4. Set: Sprite_Current faces as the leader
// (+8); the cell ahead of it, (x, z) = its high words +0x36 / +0x3A plus the
// cell step for its direction (two signed bytes at 0x66971C + 2 * +8,
// unchecked; 16-bit sums). With the leader stepping (+9 not 0) and its +0x137
// at 1: unless Area140_BlockedAhead(x, z), with x - 0x44 below 0xD and z - 6
// below 0xB (16-bit, unsigned), the script object's +7 = 2 and
// MoveCmd_Move(it, the direction); otherwise Sprite_FaceDirection(the
// direction). Then, q = (+8 >> 1) & 3 of Sprite_Current read again: the
// action button (0x90358C) not in Input_Pressed: the script position + 4.
// Pressed: the script object's +0 bit 0x40, Sprite_Current's +7 bit 8,
// Sprite_SetAnimation(Area140_ActPoses[q].animation), +0x2A the pair's
// second byte, Area140_CellHook(x, z) (its answer unread).
extern "C" void __cdecl Area140_FollowAndAct(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x66) == 0) {
        ScriptStep(4);
        return;
    }
    Sprite_Current[8] = B(::bof3::Img(ObjTrio_addr) + 0x8);
    const unsigned char* const cur = Sprite_Current;
    const unsigned char d = cur[8];
    const unsigned char* const delta = Mem(::bof3::Img(at::kCellDelta.v) + d * 2u);
    const auto x = static_cast<std::uint16_t>(static_cast<signed char>(delta[0]) + rv::As<records::SpriteObject>(cur)->x_whole);
    const auto z = static_cast<std::uint16_t>(static_cast<signed char>(delta[1]) + rv::As<records::SpriteObject>(cur)->z_whole);
    if (B(::bof3::Img(ObjTrio_addr) + 0x9) != 0 && B(::bof3::Img(ObjTrio_addr) + 0x137) == 1) {
        if (AH_CALL(Area140_BlockedAhead)(x, z) == 0 && static_cast<std::uint16_t>(x - 0x44) < 0xD &&
            static_cast<std::uint16_t>(z - 6) < 0xB) {
            MoveScript_Object[7] = 2;
            AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
        }
    } else {
        AH_CALL(Sprite_FaceDirection)(d);
    }
    const unsigned q = (Sprite_Current[8] >> 1) & 3u;
    if ((Input_Pressed & Word(Mem(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c))) == 0) {
        ScriptStep(4);
        return;
    }
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    AH_CALL(Sprite_SetAnimation)(B(::bof3::Img(Area140_ActPoses_addr) + q * 2));
    Sprite_Current[0x2A] = B(::bof3::Img(Area140_ActPoses_addr) + q * 2 + 1);
    AH_CALL(Area140_CellHook)(x, z);
}

// original 0x41F790 (called by Area140_FollowAndAct only): 0 unless
// Field_ObjectBlockedAhead(Field_ActiveMember); blocked: 1 unless the map
// byte at (x, z) is 0x89.
extern "C" unsigned char __cdecl Area140_BlockedAhead(long x, long z) {
    if (AH_CALL(Field_ObjectBlockedAhead)(Field_ActiveMember) == 0) return 0;
    if (AH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)) == 0x89) return 0;
    return 1;
}

// original 0x41F7C0 (Field_ModeTailKinds 41): by the signed state (0..11; any
// other nothing), through the twelve-entry jump table 0x41F930:
//   0   ScriptFlags_Clear40, Party_DropIn(0), Field_ScriptFlags | 0x1270, the
//       leader's +0x48 = 1, story flag 0x66, state 1;
//   1   Camera_Distance at 0x680: state 2; else + 0x40; MapView_Redraw 2;
//   2   Field_Request 0: the word 0x903850 = Input_Held, and Input_Held's
//       high nibble replaced by Area140_ButtonRemap[it] (its low nibble);
//   5   counter 3 = 1, state 6;
//   6   Camera_Distance at 0: state 7; else - 0x40; ScriptFlags_Set40 and
//       MapView_Redraw 2 either way;
//   7   ScriptFlags_Clear40, Field_ScriptFlags & 0xED8F, the leader's +0x48
//       = 0, Flags_Clear(story, 0x66), Field_EncounterThresholdRoll(0), kind and
//       state 0;
//   10  Msg_OpenScript(1), Field_Request 2, state 0xB;
//   11  Field_Request not 2: ScriptFlags_Clear40, kind and state 0;
//   3, 4, 8, 9 nothing.
extern "C" void __cdecl Area140_Tail41(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0:
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1270);
        B(::bof3::Img(ObjTrio_addr) + 0x48) = 1;
        AH_CALL(Flags_Set)(StoryFlags(), 0x66);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        if (static_cast<std::uint16_t>(Camera_Distance) == 0x680) {
            B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
            MapView_Redraw = 2;
            return;
        }
        Camera_Distance = static_cast<short>(Camera_Distance + 0x40);
        MapView_Redraw = 2;
        return;
    case 2: {
        if (Field_Request != 0) return;
        const unsigned held = Input_Held;
        SetWord(Mem(::bof3::Img(at::kScratch850.v)), held);
        Input_Held = static_cast<unsigned short>((B(::bof3::Img(Area140_ButtonRemap_addr) + (held >> 12)) << 12) | (held & 0xFFF));
        return;
    }
    case 5:
        B(::bof3::Img(at::kCounter3.v)) = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 6;
        return;
    case 6:
        if (Camera_Distance == 0) {
            B(::bof3::Img(AreaW0c_TailState_addr)) = 7;
        } else {
            Camera_Distance = static_cast<short>(Camera_Distance - 0x40);
        }
        AH_CALL(ScriptFlags_Set40)();
        MapView_Redraw = 2;
        return;
    case 7:
        AH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xED8F);
        B(::bof3::Img(ObjTrio_addr) + 0x48) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x66);
        AH_CALL(Field_EncounterThresholdRoll)(0);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 10:
        AH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 11:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    default: return;   // 3, 4, 8, 9 (the table's ret), and past 11
    }
}

// original 0x41F960 (Area_StepHook's case for area 0x8C): Cond_ByteFD 0,
// story flag 0x66 set, x exactly 0x558000, z's high word 0xA or 0xB (16-bit):
// ScriptFlags_Set40 and state 5 (the kind left as it is: tail 41's state 2
// idles under it). al 0 always.
extern "C" unsigned char __cdecl Area140_StepHook(long x, long z) {
    if (Cond_ByteFD != 0) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x66) == 0) return 0;
    if (static_cast<U>(x) != 0x558000) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(z) >> 16) - 0xA) >= 2) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0c_TailState_addr)) = 5;
    return 0;
}

// original 0x41F9B0 (Area_CellHooks, area 0x8C; and Area140_FollowAndAct):
// FindCell over Area140_CellEntries; none: al 0. Its flag (+3) set: al 0.
// With a rectangle (+5, an index into Area140_CellRects, unchecked) and a
// member count above 0 (read once): for each member record, the point one
// timed step on - (+0xC * +9 + +0x34, +0x10 * +9 + +0x38), dwords - whose
// high words less the rectangle's (x, z) are both below 2 (16-bit): the tail
// armed (ScriptFlags_Set40, kind 41, state 0xA), al 0. Otherwise: story flag
// +3 set, MoveCmd_TestFB(+0, +1); with +4, an effect record (Effect_FindFree)
// of kind 0x71 at (+0 << 16, +1 << 16) holding the flag in +0xB;
// Sound_PlayEffect(0x204); al 1.
extern "C" unsigned char __cdecl Area140_CellHook(long x, long z) {
    const unsigned i = FindCell(::bof3::Img(Area140_CellEntries_addr), at::kArea140CellCount, at::kArea140CellStride, x, z);
    if (i == at::kArea140CellCount) return 0;
    const unsigned char* const e = Mem(::bof3::Img(Area140_CellEntries_addr) + i * at::kArea140CellStride);
    if (AH_CALL(Flags_Test)(StoryFlags(), e[3]) != 0) return 0;
    const unsigned rect = e[5];
    if (rect != 0) {
        const unsigned count = Field_MemberCount;
        const unsigned char* const r = Mem(::bof3::Img(Area140_CellRects_addr) + rect * 4);
        const std::uint16_t rx = Word(r), rz = Word(r + 2);
        for (unsigned m = 0; m < count; ++m) {
            const unsigned char* const p = Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride);
            const auto* const tP = rv::AsConst<records::PartyMember>(p);
            const U n = p[9];
            const U px = static_cast<U>(tP->step_x) * n + static_cast<U>(tP->x);
            const U pz = static_cast<U>(tP->step_z) * n + static_cast<U>(tP->z);
            if (static_cast<std::uint16_t>((pz >> 16) - rz) < 2 && static_cast<std::uint16_t>((px >> 16) - rx) < 2) {
                ArmTail(0x29, 0xA);
                return 0;
            }
        }
    }
    AH_CALL(Flags_Set)(StoryFlags(), e[3]);
    AH_CALL(MoveCmd_TestFB)(e[0], e[1]);
    if (e[4] != 0) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            unsigned char* const fx = EffectAt(slot);
            auto* const tFx = rv::As<records::EffectObject>(fx);
            fx[0] = 1;
            fx[5] = 0x71;
            tFx->x = static_cast<std::int32_t>(static_cast<U>(e[0]) << 16);
            tFx->z = static_cast<std::int32_t>(static_cast<U>(e[1]) << 16);
            fx[0xB] = e[3];
        }
    }
    AH_CALL(Sound_PlayEffect)(0x204);
    return 1;
}

// original 0x41FB60 (Field_ObjectTriggers id 18; a gap of the tool, area 140's
// block): ScriptFlags_Set40, state 5 (tail 41's). al 0.
extern "C" unsigned char __cdecl Area140_Trigger18(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0c_TailState_addr)) = 5;
    return 0;
}

// original 0x41FB70 (Effect_KindHandlers[0x71]; a gap of the tool):
// Area140_Effect71States by Sprite_Current[1] (the running effect record).
extern "C" void __cdecl Area140_Effect71Run(void) {
    StateEntry("Area140_Effect71Run", ::bof3::Img(Area140_Effect71States_addr), at::kArea140EffectStateCount, Sprite_Current[1])();
}

// original 0x41FB90 (Area140_Effect71States[0]): +9 = 0xD2, +1 = 1.
extern "C" void __cdecl Area140_Effect71Start(void) {
    Sprite_Current[9] = 0xD2;
    Sprite_Current[1] = 1;
}

// original 0x41FBB0 (Area140_Effect71States[1]): +9 - 1; at 0, +1 = 2;
// Field_Request 5, +1 = 2.
extern "C" void __cdecl Area140_Effect71Count(void) {
    unsigned char* const cur = Sprite_Current;
    cur[9] = static_cast<unsigned char>(cur[9] - 1);
    if (cur[9] == 0) cur[1] = 2;
    if (Field_Request == 5) cur[1] = 2;
}

// original 0x41FBE0 (Area140_Effect71States[2]): Flags_Clear(story, +0xB);
// MoveCmd_TestFB at its high words +0x36 / +0x3A (Sprite_Current read again);
// Effect_Release (the original's tail jmp).
extern "C" void __cdecl Area140_Effect71End(void) {
    AH_CALL(Flags_Clear)(StoryFlags(), Sprite_Current[0xB]);
    const unsigned char* const cur = Sprite_Current;
    AH_CALL(MoveCmd_TestFB)(static_cast<short>(rv::As<records::EffectObject>(cur)->x_whole), static_cast<short>(rv::As<records::EffectObject>(cur)->z_whole));
    AH_CALL(Effect_Release)();
}

void Area140_Inject() {
    BOF3_INJECT(Area140_ChoiceArmTail41);
    BOF3_INJECT(Area140_FlagIfLeader89Is567);
    BOF3_INJECT(Area140_FollowAndAct);
    BOF3_INJECT(Area140_BlockedAhead);
    BOF3_INJECT(Area140_Tail41);
    BOF3_INJECT(Area140_StepHook);
    BOF3_INJECT(Area140_CellHook);
    BOF3_INJECT(Area140_Trigger18);
    BOF3_INJECT(Area140_Effect71Run);
    BOF3_INJECT(Area140_Effect71Start);
    BOF3_INJECT(Area140_Effect71Count);
    BOF3_INJECT(Area140_Effect71End);
}

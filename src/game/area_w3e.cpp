// World 3's areas 136 and 139..142: the PSX's BIN/WORLD03/AREA136 and
// AREA139..142.EMI compiled into the exe at 0x41EFE0..0x420800
// (Area_Descriptors entries 136..142; 137 and 138 have no code). Round ten,
// group AR3E: the band's 52 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w3e.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatchers through the state tables of areas 140, 141 and 142
// abort past the table (what follows is data or another table, which the
// original would jump through), docs/area_w3e.md section 6. Every call goes
// through the harness (AH_CALL), so the start-up fuzz can stand recorders in
// for ours as for the originals' copies; the callees of the group's own are
// called the same way, so each function is fuzzed alone.
#include "game/area_w3e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`) and Sprite_Objects record `slot` (0xA4, likewise).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
unsigned char* ObjectAt(unsigned slot) { return Sprite_Objects + slot * at::kObjectStride; }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(at::kFlagRow); }
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
    B(at::kTailKind) = kind;
    B(at::kTailState) = state;
}

// The first of `count` entries of `stride` bytes at `table` whose x byte is
// x's low byte, whose z byte is z's, and whose direction (the third byte's low
// nibble) is the leader's direction byte (the whole byte: 0x10 or more matches
// none); `count` when none - the search the three cell hooks open with.
unsigned FindCell(U table, unsigned count, unsigned stride, long x, long z) {
    const unsigned char pose = B(at::kLeaderPose);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const e = Mem(table + i * stride);
        if (e[0] == static_cast<unsigned char>(x) && e[1] == static_cast<unsigned char>(z) && (e[2] & 0xF) == pose) return i;
    }
    return count;
}

// Area 136's handlers 2..4, 7 and 8: Sprite_Current = `record`; an effect of
// kind `kind`, a 0, b the table byte for the character in party slot `slot`,
// at the record's words +0x2E / +0x30; a slot answered: Sprite_Current's +0xB.
void SpawnByCharacter(U record, U list, U table, unsigned char kind) {
    unsigned char* const r = Mem(record);
    const auto x = static_cast<short>(Word(r + 0x2E));
    const auto z = static_cast<short>(Word(r + 0x30));
    const unsigned char who = B(list);
    Sprite_Current = r;
    const auto b = static_cast<signed char>(B(table + who));
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, b, x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 136's handlers 1 and 5: map cells (9, 0x59..0x5B) set to `value`.
void CellsRow59(unsigned value) {
    AH_CALL(AreaMap_SetByte)(9, 0x59, value);
    AH_CALL(AreaMap_SetByte)(9, 0x5A, value);
    AH_CALL(AreaMap_SetByte)(9, 0x5B, value);
}

// Area 136's choices 0 and 1: the answer read, then the message word 0xFFFF;
// answer 0: ScriptFlags_Set40, tail kind 19 at state 0 with sub-kind `sub`.
void ChoiceTail19(unsigned char sub) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x13;
    B(at::kTailState) = 0;
    B(at::kTailSub) = sub;
}

// Area 136's choices 2 and 3: the message word from `table` by the signed
// answer (two words; an answer past them reads on in .data, unchecked);
// answer 0: counter 0 = 6.
void ChoiceMessage(U table) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetWord(Mem(at::kMessage), Word(Mem(static_cast<U>(table + static_cast<std::int32_t>(answer) * 2))));
    if (answer == 0) B(at::kCounter0) = 6;
}

// Area 141's two object shades: Sprite_Current's position moved by its step
// (+0xC to x, +0x10 to z, dwords), then the three shade bytes +0x5D..+0x5F.
void DriftStep() {
    unsigned char* cur = Sprite_Current;
    SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x34)) + static_cast<U>(Long(cur + 0xC))));
    cur = Sprite_Current;
    SetLong(cur + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x38)) + static_cast<U>(Long(cur + 0x10))));
}

}  // namespace

// ===========================================================================
// Area 136 (descriptor 0x62EA30; PSX 0x801F43B4): fifteen choices, eleven
// handlers (choice 4 + n is handler n), tail kind 19, the init. Choice 13 =
// handler 9 is 0x42A4B0, another block's.
// ===========================================================================

// original 0x41EFE0 (area 136 +0x34[0]): ChoiceTail19, sub-kind 0.
extern "C" void __cdecl Area136_ChoiceTail19Sub0(void) { ChoiceTail19(0); }

// original 0x41F010 (area 136 +0x34[1]): ChoiceTail19, sub-kind 1.
extern "C" void __cdecl Area136_ChoiceTail19Sub1(void) { ChoiceTail19(1); }

// original 0x41F040 (area 136 +0x34[2]): ChoiceMessage, Area136_ChoiceMessages2.
extern "C" void __cdecl Area136_ChoiceMessage2(void) { ChoiceMessage(at::kArea136Messages2); }

// original 0x41F070 (area 136 +0x34[3]): ChoiceMessage, Area136_ChoiceMessages3.
extern "C" void __cdecl Area136_ChoiceMessage3(void) { ChoiceMessage(at::kArea136Messages3); }

// original 0x41F0A0 (area 136 +0x34[4] = +0x3C[0]; PSX 0x801F2D68): the z one
// step ahead of Sprite_Current (+0x38 + Field_DirectionSteps[+8 & 7]'s z,
// dwords, wrapping) at most Area136_ZLimits[sub-kind] << 16 (unsigned): the
// script object's +7 = 1, MoveCmd_Move(it, 5), the script position - 2
// (the object read again after the call).
extern "C" void __cdecl Area136_MoveUntilLimit(void) {
    const unsigned char* const cur = Sprite_Current;
    const U step = static_cast<U>(Long(Mem(at::kDirectionSteps + (cur[8] & 7u) * 8 + 4)));
    const U ahead = step + static_cast<U>(Long(cur + 0x38));
    const U limit = static_cast<U>(B(at::kArea136Limits + B(at::kTailSub))) << 16;
    if (ahead > limit) return;
    MoveScript_Object[7] = 1;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    ScriptStep(-2);
}

// original 0x41F100 (area 136 +0x34[5] = +0x3C[1]; PSX 0x801F2E08): cells
// (9, 0x59..0x5B) 0x51.
extern "C" void __cdecl Area136_CellsRow51(void) { CellsRow59(0x51); }

// original 0x41F130 (area 136 +0x34[6] = +0x3C[2]; PSX 0x801F2E50): party
// record 1, effect kind 2 by Area136_EffectByCharA[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind2(void) {
    SpawnByCharacter(at::kLeader + at::kPartyStride, at::kPartyList1, at::kEffectsByCharA, 2);
}

// original 0x41F180 (area 136 +0x34[7] = +0x3C[3]; PSX 0x801F2ED0): party
// record 2, effect kind 2 by Area136_EffectByCharA[slot 2's character].
extern "C" void __cdecl Area136_SpawnMember2Kind2(void) {
    SpawnByCharacter(at::kLeader + 2 * at::kPartyStride, at::kPartyList2, at::kEffectsByCharA, 2);
}

// original 0x41F1D0 (area 136 +0x34[8] = +0x3C[4]; PSX 0x801F2F50): party
// record 1, effect kind 1 by Area136_EffectByCharB[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind1(void) {
    SpawnByCharacter(at::kLeader + at::kPartyStride, at::kPartyList1, at::kEffectsByCharB, 1);
}

// original 0x41F220 (area 136 +0x34[9] = +0x3C[5]; PSX 0x801F2FD0): cells
// (9, 0x59..0x5B) 0x10.
extern "C" void __cdecl Area136_CellsRow10(void) { CellsRow59(0x10); }

// original 0x41F250 (area 136 +0x34[10] = +0x3C[6]; PSX 0x801F3018): counter
// 0 = 0xA when the low byte of the word 0x939A00 is 0x62..0x66, else 0x1E.
extern "C" void __cdecl Area136_Counter0ByTile(void) {
    const unsigned tile = Word(Mem(at::kTile)) & 0xFFu;
    B(at::kCounter0) = tile >= 0x62 && tile <= 0x66 ? 0xA : 0x1E;
}

// original 0x41F280 (area 136 +0x34[11] = +0x3C[7]; PSX 0x801F3048): party
// record 1, effect kind 3 by Area136_EffectByCharB[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind3(void) {
    SpawnByCharacter(at::kLeader + at::kPartyStride, at::kPartyList1, at::kEffectsByCharB, 3);
}

// original 0x41F2D0 (area 136 +0x34[12] = +0x3C[8]; PSX 0x801F30C8): the
// leader's record, effect kind 1 by Area136_EffectByCharB[slot 1's character]
// (slot 1's, not slot 0's: as read).
extern "C" void __cdecl Area136_SpawnLeaderKind1(void) {
    SpawnByCharacter(at::kLeader, at::kPartyList1, at::kEffectsByCharB, 1);
}

// original 0x41F320 (area 136 +0x34[14] = +0x3C[10], PSX 0x801F3170; area 119
// +0x34[2] = +0x3C[1], area 131 +0x3C[7], area 188 +0x34[5] = +0x3C[1]):
// Camera_ShiftY + 2, MapView_Redraw 2.
extern "C" void __cdecl Area136_ShiftCameraUp2(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 2);
    MapView_Redraw = 2;
}

// original 0x41F340 (Field_ModeTailKinds 19): by the signed state (0..13; any
// other nothing), through the byte map 0x41F4D0 and the eight-entry jump table
// 0x41F4B0 inside the function:
//   0   KeyItem_Has(6): state 10 when held, else 1;
//   1   Field_Request not 2: Msg_OpenScript(0x83), Field_Request 2, state 2;
//   2   Field_Request not 2: kind, state, sub-kind 0, ScriptFlags_Clear40;
//   10  Flags_Set(the flag row, 0x2E); then Field_Request not 2:
//       Msg_OpenScript(0x82), Field_Request 2, Flags_Toggle(story, 0x29),
//       state 0xB;
//   11  Field_Request not 2: Party_DropIn(0), state 0xC;
//   12  counter 3 at 0x28: Field_ChangeArea(0x88, ...) to (0x330000, 0x470000)
//       with flags 0x81 for sub-kind 0, else (0x80000, 0x220000) with 0x82;
//       state 0xD;
//   13  counter 3 at 0: ScriptFlags_Clear40, kind, state, sub-kind 0,
//       Flags_Clear(the flag row, read after the call, 0x2E);
//   3..9 nothing.
extern "C" void __cdecl Area136_Tail19(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0:
        B(at::kTailState) = AH_CALL(KeyItem_Has)(6) != 0 ? 10 : 1;
        return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x83);
        Field_Request = 2;
        B(at::kTailState) = 2;
        return;
    case 2:
        if (Field_Request == 2) return;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kTailSub) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 10:
        AH_CALL(Flags_Set)(FlagRow(), 0x2E);
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x82);
        Field_Request = 2;
        AH_CALL(Flags_Toggle)(StoryFlags(), 0x29);
        B(at::kTailState) = 0xB;
        return;
    case 11:
        if (Field_Request == 2) return;
        AH_CALL(Party_DropIn)(0);
        B(at::kTailState) = 0xC;
        return;
    case 12:
        if (B(at::kCounter3) != 0x28) return;
        if (B(at::kTailSub) == 0)
            AH_CALL(Field_ChangeArea)(0x88, 0x330000, 0x470000, 0x81);
        else
            AH_CALL(Field_ChangeArea)(0x88, 0x80000, 0x220000, 0x82);
        B(at::kTailState) = 0xD;
        return;
    case 13: {
        if (B(at::kCounter3) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        unsigned char* const row = FlagRow();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kTailSub) = 0;
        AH_CALL(Flags_Clear)(row, 0x2E);
        return;
    }
    default: return;   // 3..9 (the byte map's entry 7, a ret), and past 13
    }
}

// original 0x41F4E0 (area 136 +0x40; PSX 0x801F33BC): the byte 0x905E68 at 1
// and Cond_ByteFD 4: Sprite_ObjectsExtra[0] +0x83 = 2; at 4 and Cond_ByteFD
// 1: = 4.
extern "C" void __cdecl Area136_InitExtraObject(void) {
    const unsigned char zone = B(at::kLastZone);
    if (zone == 1) {
        if (Cond_ByteFD == 4) B(at::kExtra0_83) = 2;
        return;
    }
    if (zone == 4 && Cond_ByteFD == 1) B(at::kExtra0_83) = 4;
}

// ===========================================================================
// Area 139 (descriptor 0x62ED78; PSX 0x801F45A0): one handler, the cell hook.
// ===========================================================================

// original 0x41F510 (area 139 +0x3C[0]; PSX 0x801F42F4): the active member's
// +0x80 bit 0 cleared; the leader's +0x89 at 5: story flag 0x4D,
// MoveCmd_TestFB(4, 0xD), Sprite_Current's +0 = 0 (read after the calls).
extern "C" void __cdecl Area139_FlagIfLeader89Is5(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kLeader89) != 5) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x4D);
    AH_CALL(MoveCmd_TestFB)(4, 0xD);
    Sprite_Current[0] = 0;
}

// original 0x41F550 (Area_CellHooks, area 0x8B): FindCell over
// Area139_CellEntries; a match: Flags_Toggle(story, its flag byte),
// Sound_PlayEffect(0x204), al 1; none: al 0.
extern "C" unsigned char __cdecl Area139_CellHook(long x, long z) {
    const unsigned i = FindCell(at::kArea139Cells, at::kArea139CellCount, 4, x, z);
    if (i == at::kArea139CellCount) return 0;
    AH_CALL(Flags_Toggle)(StoryFlags(), B(at::kArea139Cells + i * 4 + 3));
    AH_CALL(Sound_PlayEffect)(0x204);
    return 1;
}

// ===========================================================================
// Area 140 (descriptor 0x62EF98; PSX 0x801F4A28): one choice (= handler 2),
// three handlers, tail kind 41, the step hook, the cell hook, object trigger
// 18, effect kind 0x71 and its three states.
// ===========================================================================

// original 0x41F5C0 (area 140 +0x34[0] = +0x3C[2]; PSX 0x801F3D4C): answer 0:
// message 4; answer 1: ScriptFlags_Set40, tail kind 41 at state 0, message
// 0xFFFF; any other: message 0xFFFF.
extern "C" void __cdecl Area140_ChoiceArmTail41(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
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
    const unsigned char v = B(at::kLeader89);
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
    Sprite_Current[8] = B(at::kLeaderPose);
    const unsigned char* const cur = Sprite_Current;
    const unsigned char d = cur[8];
    const unsigned char* const delta = Mem(at::kCellDelta + d * 2u);
    const auto x = static_cast<std::uint16_t>(static_cast<signed char>(delta[0]) + Word(cur + 0x36));
    const auto z = static_cast<std::uint16_t>(static_cast<signed char>(delta[1]) + Word(cur + 0x3A));
    if (B(at::kLeaderSteps) != 0 && B(at::kLeader137) == 1) {
        if (AH_CALL(Area140_BlockedAhead)(x, z) == 0 && static_cast<std::uint16_t>(x - 0x44) < 0xD &&
            static_cast<std::uint16_t>(z - 6) < 0xB) {
            MoveScript_Object[7] = 2;
            AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
        }
    } else {
        AH_CALL(Sprite_FaceDirection)(d);
    }
    const unsigned q = (Sprite_Current[8] >> 1) & 3u;
    if ((Input_Pressed & Word(Mem(at::kButtonMap6))) == 0) {
        ScriptStep(4);
        return;
    }
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    AH_CALL(Sprite_SetAnimation)(B(at::kArea140Poses + q * 2));
    Sprite_Current[0x2A] = B(at::kArea140Poses + q * 2 + 1);
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
//       = 0, Flags_Clear(story, 0x66), Field_ZoneCounterRoll(0), kind and
//       state 0;
//   10  Msg_OpenScript(1), Field_Request 2, state 0xB;
//   11  Field_Request not 2: ScriptFlags_Clear40, kind and state 0;
//   3, 4, 8, 9 nothing.
extern "C" void __cdecl Area140_Tail41(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0:
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1270);
        B(at::kLeader48) = 1;
        AH_CALL(Flags_Set)(StoryFlags(), 0x66);
        B(at::kTailState) = 1;
        return;
    case 1:
        if (static_cast<std::uint16_t>(Camera_Distance) == 0x680) {
            B(at::kTailState) = 2;
            MapView_Redraw = 2;
            return;
        }
        Camera_Distance = static_cast<short>(Camera_Distance + 0x40);
        MapView_Redraw = 2;
        return;
    case 2: {
        if (Field_Request != 0) return;
        const unsigned held = Input_Held;
        SetWord(Mem(at::kScratch850), held);
        Input_Held = static_cast<unsigned short>((B(at::kArea140Remap + (held >> 12)) << 12) | (held & 0xFFF));
        return;
    }
    case 5:
        B(at::kCounter3) = 1;
        B(at::kTailState) = 6;
        return;
    case 6:
        if (Camera_Distance == 0) {
            B(at::kTailState) = 7;
        } else {
            Camera_Distance = static_cast<short>(Camera_Distance - 0x40);
        }
        AH_CALL(ScriptFlags_Set40)();
        MapView_Redraw = 2;
        return;
    case 7:
        AH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xED8F);
        B(at::kLeader48) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x66);
        AH_CALL(Field_ZoneCounterRoll)(0);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 10:
        AH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        B(at::kTailState) = 0xB;
        return;
    case 11:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
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
    B(at::kTailState) = 5;
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
    const unsigned i = FindCell(at::kArea140Cells, at::kArea140CellCount, at::kArea140CellStride, x, z);
    if (i == at::kArea140CellCount) return 0;
    const unsigned char* const e = Mem(at::kArea140Cells + i * at::kArea140CellStride);
    if (AH_CALL(Flags_Test)(StoryFlags(), e[3]) != 0) return 0;
    const unsigned rect = e[5];
    if (rect != 0) {
        const unsigned count = Field_MemberCount;
        const unsigned char* const r = Mem(at::kArea140Rects + rect * 4);
        const std::uint16_t rx = Word(r), rz = Word(r + 2);
        for (unsigned m = 0; m < count; ++m) {
            const unsigned char* const p = Mem(at::kLeader + m * at::kPartyStride);
            const U n = p[9];
            const U px = static_cast<U>(Long(p + 0xC)) * n + static_cast<U>(Long(p + 0x34));
            const U pz = static_cast<U>(Long(p + 0x10)) * n + static_cast<U>(Long(p + 0x38));
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
            fx[0] = 1;
            fx[5] = 0x71;
            SetLong(fx + 0x34, static_cast<std::int32_t>(static_cast<U>(e[0]) << 16));
            SetLong(fx + 0x38, static_cast<std::int32_t>(static_cast<U>(e[1]) << 16));
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
    B(at::kTailState) = 5;
    return 0;
}

// original 0x41FB70 (Effect_KindHandlers[0x71]; a gap of the tool):
// Area140_Effect71States by Sprite_Current[1] (the running effect record).
extern "C" void __cdecl Area140_Effect71Run(void) {
    StateEntry("Area140_Effect71Run", at::kArea140EffectStates, at::kArea140EffectStateCount, Sprite_Current[1])();
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
    AH_CALL(MoveCmd_TestFB)(static_cast<short>(Word(cur + 0x36)), static_cast<short>(Word(cur + 0x3A)));
    AH_CALL(Effect_Release)();
}

// ===========================================================================
// Area 141 (descriptor 0x62F5A0; PSX 0x801F4D50): six choices (choice 2 + n
// is handler n; choice 1 is 0x403050, world 0's), four handlers, two state
// tables, the init, tail kind 52, the cell hook, object trigger 57, the
// cell-run painter, and five object placers chapter 14's code calls.
// ===========================================================================

// original 0x41FC10 (area 141 +0x34[2] = +0x3C[0]; PSX 0x801F3588):
// Sprite_Current's +0 bit 0x20 cleared, +0x5F..+0x5C = 0 (the pointer read
// again for each), Sprite_ReleaseTint(it).
extern "C" void __cdecl Area141_ShadeOff(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5C] = 0;
    AH_CALL(Sprite_ReleaseTint)(Sprite_Current);
}

// original 0x41FC50 (area 141 +0x34[3] = +0x3C[1]; PSX 0x801F35E4): d =
// (Sprite_Current's x - 0x450000) >> 15 (arithmetic); 0: nothing. Else the
// script object's +7 = |d| (a byte), Sprite_Current's +8 = 7, and the move
// in direction +8: MoveCmd_MoveKind2 when Sprite_Current is Sprite_Kind2,
// else MoveCmd_Move(the script object, it).
extern "C" void __cdecl Area141_MoveToX45(void) {
    const std::int32_t d = static_cast<std::int32_t>(static_cast<U>(Long(Sprite_Current + 0x34)) - 0x450000u) >> 15;
    if (d == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(d < 0 ? -d : d);
    Sprite_Current[8] = 7;
    unsigned char* const cur = Sprite_Current;
    if (cur == Sprite_Kind2) {
        AH_CALL(MoveCmd_MoveKind2)(cur[8]);
        return;
    }
    AH_CALL(MoveCmd_Move)(MoveScript_Object, cur[8]);
}

// original 0x41FCB0 (area 141 +0x34[4] = +0x3C[2]; PSX 0x801F3688):
// Area141_ShadeDownStates by Sprite_Current[4]. Unchecked there: indices 2
// and 3 read Area141_ShadeUpStates, which follows, and run its states (kept);
// past those, data (ours aborts).
extern "C" void __cdecl Area141_RunShadeDown(void) {
    StateEntry("Area141_RunShadeDown", at::kArea141ShadeDownStates, at::kArea141ShadeDownReach, Sprite_Current[4])();
}

// original 0x41FCD0 (Area141_ShadeDownStates[0]): +0 bit 0x20, +0x5F..+0x5D =
// 0xC4, +0x5C = 1; Area141_ShadeDownStep; +4 = 1.
extern "C" void __cdecl Area141_ShadeDownStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0x5F] = 0xC4;
    Sprite_Current[0x5E] = 0xC4;
    Sprite_Current[0x5D] = 0xC4;
    Sprite_Current[0x5C] = 1;
    AH_CALL(Area141_ShadeDownStep)();
    Sprite_Current[4] = 1;
}

// original 0x41FD20 (Area141_ShadeDownStates[1]): DriftStep; each of
// +0x5D..+0x5F not 0x80 less 2; all three at 0x80: +0 bit 0x40, +4 = 0; else
// the script position - 2.
extern "C" void __cdecl Area141_ShadeDownStep(void) {
    DriftStep();
    unsigned char* const cur = Sprite_Current;
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (cur[k] != 0x80) cur[k] = static_cast<unsigned char>(cur[k] - 2);
    if (cur[0x5D] == 0x80 && cur[0x5E] == 0x80 && cur[0x5F] == 0x80) {
        cur[0] = static_cast<unsigned char>(cur[0] | 0x40);
        cur[4] = 0;
        return;
    }
    ScriptStep(-2);
}

// original 0x41FDB0 (area 141 +0x34[5] = +0x3C[3]; PSX 0x801F3838):
// Area141_ShadeUpStates by Sprite_Current[4].
extern "C" void __cdecl Area141_RunShadeUp(void) {
    StateEntry("Area141_RunShadeUp", at::kArea141ShadeUpStates, at::kArea141StateCount, Sprite_Current[4])();
}

// original 0x41FDD0 (Area141_ShadeUpStates[0]): +0 bit 0x20 set, bit 0x40
// cleared, +0x5C = 1, +0x5F..+0x5D = 0x80; Area141_ShadeUpStep; +4 = 1.
extern "C" void __cdecl Area141_ShadeUpStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5D] = 0x80;
    AH_CALL(Area141_ShadeUpStep)();
    Sprite_Current[4] = 1;
}

// original 0x41FE20 (Area141_ShadeUpStates[1]): DriftStep; each of
// +0x5D..+0x5F below 0xC0 (signed bytes: below -64) plus 4; all three at 0xC0:
// +0 bit 0x20 cleared, +0x5C, +0x5F..+0x5D and +4 = 0; else the active
// member's word +0x8A - 2.
extern "C" void __cdecl Area141_ShadeUpStep(void) {
    DriftStep();
    unsigned char* const cur = Sprite_Current;
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (static_cast<signed char>(cur[k]) < static_cast<signed char>(0xC0)) cur[k] = static_cast<unsigned char>(cur[k] + 4);
    if (cur[0x5D] == 0xC0 && cur[0x5E] == 0xC0 && cur[0x5F] == 0xC0) {
        cur[0] = static_cast<unsigned char>(cur[0] & 0xDF);
        cur[0x5C] = 0;
        cur[0x5F] = 0;
        cur[0x5E] = 0;
        cur[0x5D] = 0;
        cur[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x41FEE0 (area 141 +0x34[0]): story flag 0x7D; then the answer
// (read after the call) 0: ScriptFlags_Set40, Kind2_Place(3),
// Area141_PaintCells(run 8, off), tail kind 52 at state 0x32, message 5;
// else message 6.
extern "C" void __cdecl Area141_ChoiceFlag7D(void) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x7D);
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(6);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    AH_CALL(Kind2_Place)(3);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(8)), 0);
    B(at::kTailKind) = 0x34;
    B(at::kTailState) = 0x32;
    SetMessage(5);
}

// Area 141's init and tail: the two runs `a`, `a + 1` painted on (flag clear)
// or off (flag set) - the init's loops of two.
static void PaintPairByFlag(unsigned flag, unsigned a) {
    const unsigned char on = AH_CALL(Flags_Test)(StoryFlags(), flag) == 0 ? 1 : 0;
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a)), on);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a + 1)), on);
}

// original 0x41FF40 (area 141 +0x40; PSX 0x801F3B04): Cond_ByteFD 0: runs 0
// and 1 on. Cond_ByteFD (read again) 1: runs 2, 3 by story flag 0x7A, runs 4,
// 5 by 0x7B, runs 6, 7 by 0x7C (on while the flag is clear). Cond_ByteFD
// (again) 2: run 8 on.
extern "C" void __cdecl Area141_InitCells(void) {
    if (Cond_ByteFD == 0) {
        AH_CALL(Area141_PaintCells)(Mem(at::Run(0)), 1);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(1)), 1);
    }
    if (Cond_ByteFD == 1) {
        PaintPairByFlag(0x7A, 2);
        PaintPairByFlag(0x7B, 4);
        PaintPairByFlag(0x7C, 6);
    }
    if (Cond_ByteFD == 2) AH_CALL(Area141_PaintCells)(Mem(at::Run(8)), 1);
}

// Tail 52's three flag states (0x1E, 0x28 and the choice's 0xE for 0x7A read
// apart): the flag set already: state 0x14; else set, runs `a`, `a + 1` off,
// the timer 0x3C, state `next`.
static void FlagRunsOff(unsigned flag, unsigned a, unsigned char next) {
    if (AH_CALL(Flags_Test)(StoryFlags(), flag) != 0) {
        B(at::kTailState) = 0x14;
        return;
    }
    AH_CALL(Flags_Set)(StoryFlags(), flag);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a)), 0);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a + 1)), 0);
    SetWord(Mem(at::kTailTimer), 0x3C);
    B(at::kTailState) = next;
}
// The word timer less 1; true when it reaches 0 (16-bit).
static bool TimerOut() {
    const auto t = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
    SetWord(Mem(at::kTailTimer), t);
    return t == 0;
}

// original 0x420060 (Field_ModeTailKinds 52): by the signed state (0..0x33;
// any other nothing), through the byte map 0x420328 and the fifteen-entry jump
// table 0x4202EC inside the function:
//   0     Field_Request not 2: ScriptFlags_Clear40, Party_DropIn(5), kind and
//         state 0;
//   0xA   Msg_OpenScript(3), Field_ScriptFlags' low byte | 7, Field_Request
//         2, the timer 0, state 0xB;
//   0xB   Field_Request not 2: state 0xC;
//   0xC   the timer + 1 (16-bit); at 0x1C2 or more state 0xD; else a button
//         pressed (Input_Pressed not 0): state 0x14;
//   0xD   Field_Request not 2: Msg_OpenScript(4), Field_Request 2,
//         Kind2_Place(2), state 0xE;
//   0xE   counter 0 at 1: story flag 0x7A, runs 2, 3 off, the timer 0x3C,
//         state 0xF;
//   0xF   the timer - 1 at 0: Field_Kind2X / Z = the leader's x / z, state
//         0x10;
//   0x10  Field_Kind2Hold 0: the message box's flag byte | 0x80, state 0x14;
//   0x14  ScriptFlags_Clear40, Field_ScriptFlags & 0xFFF8, kind and state 0;
//   0x1E  FlagRunsOff(0x7B, runs 4, 5, state 0x1F);
//   0x1F, 0x29  the timer - 1 at 0: state 0x14;
//   0x28  FlagRunsOff(0x7C, runs 6, 7, state 0x29);
//   0x32  counter 0 at 1: Cond_ByteFE 1, the timer 0x3C, state 0x33;
//   0x33  the timer - 1 at 0: kind and state 0, counter 0 + 1;
//   the rest nothing.
extern "C" void __cdecl Area141_Tail52(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(5);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0xA:
        AH_CALL(Msg_OpenScript)(3);
        B(at::kScriptFlagsLow) = static_cast<unsigned char>(B(at::kScriptFlagsLow) | 7);
        Field_Request = 2;
        SetWord(Mem(at::kTailTimer), 0);
        B(at::kTailState) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        B(at::kTailState) = 0xC;
        return;
    case 0xC: {
        const auto t = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) + 1);
        SetWord(Mem(at::kTailTimer), t);
        if (t >= 0x1C2) {
            B(at::kTailState) = 0xD;
            return;
        }
        if (Input_Pressed != 0) B(at::kTailState) = 0x14;
        return;
    }
    case 0xD:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(4);
        Field_Request = 2;
        AH_CALL(Kind2_Place)(2);
        B(at::kTailState) = 0xE;
        return;
    case 0xE:
        if (B(at::kCounter0) != 1) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x7A);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(2)), 0);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(3)), 0);
        SetWord(Mem(at::kTailTimer), 0x3C);
        B(at::kTailState) = 0xF;
        return;
    case 0xF:
        if (!TimerOut()) return;
        Field_Kind2X = Long(Mem(at::kLeader + 0x34));
        Field_Kind2Z = Long(Mem(at::kLeader + 0x38));
        B(at::kTailState) = 0x10;
        return;
    case 0x10:
        if (Field_Kind2Hold != 0) return;
        B(at::kMsgFlags) = static_cast<unsigned char>(B(at::kMsgFlags) | 0x80);
        B(at::kTailState) = 0x14;
        return;
    case 0x14:
        AH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFF8);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0x1E: FlagRunsOff(0x7B, 4, 0x1F); return;
    case 0x1F: case 0x29:
        if (TimerOut()) B(at::kTailState) = 0x14;
        return;
    case 0x28: FlagRunsOff(0x7C, 6, 0x29); return;
    case 0x32:
        if (B(at::kCounter0) != 1) return;
        Cond_ByteFE = 1;
        SetWord(Mem(at::kTailTimer), 0x3C);
        B(at::kTailState) = 0x33;
        return;
    case 0x33: {
        if (!TimerOut()) return;
        const unsigned char c = B(at::kCounter0);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kCounter0) = static_cast<unsigned char>(c + 1);
        return;
    }
    default: return;   // the byte map's entry 14 (a ret), and past 0x33
    }
}

// original 0x420360 (Area_CellHooks, area 0x8D): FindCell over
// Area141_CellEntries; a match: ScriptFlags_Set40, tail kind 52 at the
// entry's state (+3), al 1; none: al 0.
extern "C" unsigned char __cdecl Area141_CellHook(long x, long z) {
    const unsigned i = FindCell(at::kArea141Cells, at::kArea141CellCount, 4, x, z);
    if (i == at::kArea141CellCount) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailState) = B(at::kArea141Cells + i * 4 + 3);
    B(at::kTailKind) = 0x34;
    return 1;
}

// original 0x4203C0 (Field_ObjectTriggers id 57; a gap of the tool, area
// 141's block): by the object's word +0x88: 0xC000 runs 0 and 1 off; 0xC001
// ScriptFlags_Set40, Party_HealJoined, tail kind 52 at state 0; 0xC002
// ScriptFlags_Set40, tail kind 52 at state 0xA; any other nothing. al 0.
extern "C" unsigned char __cdecl Area141_Trigger57(unsigned char* object, unsigned char*) {
    switch (Word(object + 0x88)) {
    case 0xC000:
        AH_CALL(Area141_PaintCells)(Mem(at::Run(0)), 0);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(1)), 0);
        return 0;
    case 0xC001:
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Party_HealJoined)();
        B(at::kTailKind) = 0x34;
        B(at::kTailState) = 0;
        return 0;
    case 0xC002:
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0x34;
        B(at::kTailState) = 0xA;
        return 0;
    default: return 0;
    }
}

// original 0x420430 (called by area 141's choice 0, init, tail and trigger
// 57): a run of n = run[2] & 0x7F map cells from (run[0], run[1]) - along x
// with bit 0x80, else along z - each AreaMap_SetByte to run[3] when `on`
// (its low byte, read for each cell) is not 0, else run[4]. n 0: none.
extern "C" void __cdecl Area141_PaintCells(const unsigned char* run, unsigned on) {
    const unsigned char n = run[2] & 0x7F;
    const bool along_x = (run[2] & 0x80) != 0;
    for (unsigned char i = 0; i < n; ++i) {
        const unsigned value = static_cast<unsigned char>(on) != 0 ? run[3] : run[4];
        if (along_x)
            AH_CALL(AreaMap_SetByte)(static_cast<unsigned>(run[0] + i), run[1], value);
        else
            AH_CALL(AreaMap_SetByte)(run[0], static_cast<unsigned>(run[1] + i), value);
    }
}

// Area 141's placers: a free object (Sprite_FindFree) marked live (+0 = 1),
// the event ops' object word 0x903850 = its index, the event op on `script`.
static void PlaceOn(unsigned char slot, U script, bool op6x) {
    ObjectAt(slot)[0] = 1;
    SetWord(Mem(at::kScratch850), slot);
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(script));
    else
        AH_CALL(EventOp_0x)(Mem(script));
}
// Two free objects, or none: the first is marked live before the second is
// looked for, and let go (+0 = 0) when there is no second. Then the first's
// word 0x903850 is set before the second is marked live.
static bool TwoFree(unsigned char& a, unsigned char& b) {
    a = AH_CALL(Sprite_FindFree)();
    if (a == 0xFF) return false;
    ObjectAt(a)[0] = 1;
    b = AH_CALL(Sprite_FindFree)();
    if (b == 0xFF) {
        ObjectAt(a)[0] = 0;
        return false;
    }
    return true;
}
// The pair's placement as the originals order it: 0x903850 = a, b live, the
// op on `first` (for a); [animation]; 0x903850 = b, the op on `second`.
static void PlacePair(unsigned char a, unsigned char b, U first, U second, bool op6x, int anim_a, int anim_b) {
    SetWord(Mem(at::kScratch850), a);
    ObjectAt(b)[0] = 1;
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(first));
    else
        AH_CALL(EventOp_0x)(Mem(first));
    if (anim_a >= 0) AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim_a), 0);
    SetWord(Mem(at::kScratch850), b);
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(second));
    else
        AH_CALL(EventOp_0x)(Mem(second));
    if (anim_b >= 0) AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim_b), 0);
}

// original 0x4204D0 (called by chapter 14's 0x567145; a gap of the tool): two
// objects placed by EventOp_6x on the scripts 0x62F630 / 0x62F640, animations
// 0x6B and 0x58 after each.
extern "C" void __cdecl Area141_PlacePairAnimated(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, at::kArea141Script630, at::kArea141Script640, true, 0x6B, 0x58);
}

// original 0x420580 (called by chapter 14's 0x567179; a gap): one object by
// EventOp_6x on 0x62F650, animation 3.
extern "C" void __cdecl Area141_PlaceOneAnimated(void) {
    const unsigned char s = AH_CALL(Sprite_FindFree)();
    if (s == 0xFF) return;
    PlaceOn(s, at::kArea141Script650, true);
    AH_CALL(Sprite_SetAnimationAt)(3, 0);
}

// original 0x4205D0 (called by chapter 14's 0x5648B2; a gap): two objects by
// EventOp_6x on 0x62F660 / 0x62F670.
extern "C" void __cdecl Area141_PlacePair6x(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, at::kArea141Script660, at::kArea141Script670, true, -1, -1);
}

// original 0x420670 (called by chapter 14's 0x5671AC; a gap): two objects by
// EventOp_0x on 0x62F680 / 0x62F691.
extern "C" void __cdecl Area141_PlacePair0x(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, at::kArea141Script680, at::kArea141Script691, false, -1, -1);
}

// original 0x420710 (called by chapter 14's 0x5671E0; a gap): one object by
// EventOp_0x on 0x62F6A8.
extern "C" void __cdecl Area141_PlaceOne0x(void) {
    const unsigned char s = AH_CALL(Sprite_FindFree)();
    if (s == 0xFF) return;
    PlaceOn(s, at::kArea141Script6A8, false);
}

// ===========================================================================
// Area 142 (descriptor 0x62FC98; PSX 0x801F3B38): one handler and its two
// states.
// ===========================================================================

// original 0x420760 (area 142 +0x3C[0]; PSX 0x801F3414): Area142_States by
// Sprite_Current[4].
extern "C" void __cdecl Area142_RunWait(void) {
    StateEntry("Area142_RunWait", at::kArea142States, at::kArea142StateCount, Sprite_Current[4])();
}

// original 0x420780 (Area142_States[0]): counter 0 at 1: nothing. Else
// Field_Request 3: +0 bit 0x40, +4 = 1; the script position - 2.
extern "C" void __cdecl Area142_WaitRequest3(void) {
    if (B(at::kCounter0) == 1) return;
    if (Field_Request == 3) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[4] = 1;
    }
    ScriptStep(-2);
}

// original 0x4207C0 (Area142_States[1]): Field_Request not 3: +0 bit 0x40
// cleared, Sprite_SetAnimation(+8), +4 = 0 (the pointer read again after the
// call). The script position - 2.
extern "C" void __cdecl Area142_WaitRequestEnd(void) {
    if (Field_Request != 3) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
        AH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        Sprite_Current[4] = 0;
    }
    ScriptStep(-2);
}

void AreaW3e_Inject() {
    if (bof3::WantsShadow("area_w3e")) area_w3e::SelfTest();
    BOF3_INJECT(Area136_ChoiceTail19Sub0);
    BOF3_INJECT(Area136_ChoiceTail19Sub1);
    BOF3_INJECT(Area136_ChoiceMessage2);
    BOF3_INJECT(Area136_ChoiceMessage3);
    BOF3_INJECT(Area136_MoveUntilLimit);
    BOF3_INJECT(Area136_CellsRow51);
    BOF3_INJECT(Area136_SpawnMember1Kind2);
    BOF3_INJECT(Area136_SpawnMember2Kind2);
    BOF3_INJECT(Area136_SpawnMember1Kind1);
    BOF3_INJECT(Area136_CellsRow10);
    BOF3_INJECT(Area136_Counter0ByTile);
    BOF3_INJECT(Area136_SpawnMember1Kind3);
    BOF3_INJECT(Area136_SpawnLeaderKind1);
    BOF3_INJECT(Area136_ShiftCameraUp2);
    BOF3_INJECT(Area136_Tail19);
    BOF3_INJECT(Area136_InitExtraObject);
    BOF3_INJECT(Area139_FlagIfLeader89Is5);
    BOF3_INJECT(Area139_CellHook);
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
    BOF3_INJECT(Area141_ShadeOff);
    BOF3_INJECT(Area141_MoveToX45);
    BOF3_INJECT(Area141_RunShadeDown);
    BOF3_INJECT(Area141_ShadeDownStart);
    BOF3_INJECT(Area141_ShadeDownStep);
    BOF3_INJECT(Area141_RunShadeUp);
    BOF3_INJECT(Area141_ShadeUpStart);
    BOF3_INJECT(Area141_ShadeUpStep);
    BOF3_INJECT(Area141_ChoiceFlag7D);
    BOF3_INJECT(Area141_InitCells);
    BOF3_INJECT(Area141_Tail52);
    BOF3_INJECT(Area141_CellHook);
    BOF3_INJECT(Area141_Trigger57);
    BOF3_INJECT(Area141_PaintCells);
    BOF3_INJECT(Area141_PlacePairAnimated);
    BOF3_INJECT(Area141_PlaceOneAnimated);
    BOF3_INJECT(Area141_PlacePair6x);
    BOF3_INJECT(Area141_PlacePair0x);
    BOF3_INJECT(Area141_PlaceOne0x);
    BOF3_INJECT(Area142_RunWait);
    BOF3_INJECT(Area142_WaitRequest3);
    BOF3_INJECT(Area142_WaitRequestEnd);
}

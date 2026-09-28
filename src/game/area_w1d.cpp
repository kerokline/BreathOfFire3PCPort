// World 1's areas 53, 55..57 and 59..64: the PSX's BIN/WORLD01/AREA053..064.EMI
// compiled into the exe at 0x40AB00..0x40B8C0 (Area_Descriptors entries 53..64;
// areas 54 and 58 have no code). Round ten, group AR1D: the band's 47
// functions, none ours before, each read to its last instruction with
// capstone (2026-09-28) and taken through the area harness (area_harness.h).
// docs/area_w1d.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// two dispatchers through an area's .data state table abort past the table
// where the original would jump through whatever the next dword holds (the
// owner's rule for an unchecked index, round9 doc section 6; nothing measured
// reaches it). Reads by an unchecked index into the areas' .data tables are
// kept (they stay in .data). Every call goes through the harness (AH_CALL /
// AH_AT), so the start-up fuzz can stand recorders in for ours as for the
// originals' copies.
#include "game/area_w1d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(at::kChoiceAnswer); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
// A message and the mark 0x9398CF = 6 beside it (areas 55, 59, 61).
void MessageMarked(unsigned id) {
    SetMessage(id);
    B(at::kAnswerMark) = 6;
}
// The byte a table indexed by the s8 answer holds (`movsx eax, answer`).
std::uint32_t BySignedAnswer(std::uint32_t table, unsigned stride, unsigned offset) {
    const auto answer = static_cast<signed char>(Answer());
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + answer * static_cast<int>(stride) + static_cast<int>(offset));
}

// The pointer 0x903804 (a field object), read afresh at each use.
unsigned char* Focus() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kFocusObject)))));
}

// Areas 55, 61's object triggers 23 / 21 and area 59's 24 begin so:
// ScriptFlags_Set40, the mode tail kind 4.
void Tail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = sub;
}

// Areas 56 and 57's object triggers 52 / 53 (0x40B1D0, 0x40B2F0): story flag
// `flag` set; when flags 0x6D..0x70 are all set, flag 0x71 too (area 34's
// trigger 51 is the same with 0x6D). Answers 0 in al.
unsigned char FlagsGather(unsigned flag) {
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), flag);
    for (unsigned f = 0x6D; f <= 0x70; ++f)
        if (!AH_CALL(Flags_Test)(Mem(at::kStoryFlags), f)) return 0;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x71);
    return 0;
}

// ---- area 56 ----

// The leader's state bytes +1 = 2, +2 = 7, +3 = 0, then MoveCmd_TestFB at
// the leader's cell (the high words of x and z, read before the stores; the
// answer is not read).
void LeaderPose27() {
    const std::uint16_t z = Word(Mem(at::kLeader + 0x3A));
    const std::uint16_t x = Word(Mem(at::kLeader + 0x36));
    B(at::kLeader + 1) = 2;
    B(at::kLeader + 2) = 7;
    B(at::kLeader + 3) = 0;
    AH_CALL(MoveCmd_TestFB)(static_cast<short>(x), static_cast<short>(z));
}

// Area 56's choices 0..6 (0x40AC10..0x40AEB0): message 0xFFFF; the answer at
// `pose_on`: LeaderPose27; the answer (read again) at `flag_on`: Cond_Flags
// row 3's flag `flag` set; the answer (read again) not 2: sound 0x208.
void ChoicePoseFlag(unsigned char pose_on, unsigned char flag_on, unsigned flag) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == pose_on) LeaderPose27();
    if (Answer() == flag_on) AH_CALL(Flags_Set)(Mem(at::kCondRow3), flag);
    if (Answer() == 2) return;
    AH_CALL(Sound_PlayEffect)(0x208);
}

// Area 56's fall (Area56_FallStates): the running object's word +0x3E the
// height, +0x14 its rise, +0x20 the rise's step; each Sprite_Current read
// afresh, as the originals read it.
void FieldStatePace() { AddWord(Field_State + 0x12E, -2); }

// ---- areas 57, 59 ----

// Area 57's handlers 0 and 1 (0x40B230, 0x40B280): Effect_Spawn(kind, 0,
// kinds[the first party list byte], the leader's words +0x2E, +0x30) with
// Sprite_Current made the leader's record (and left so); an answer other than
// 0xFF to Sprite_Current[0xB], read again after the call. The list byte
// indexes the table unchecked, as the original's.
void SpawnAtLeader(unsigned char kind, std::uint32_t kinds) {
    const auto z = static_cast<short>(Word(Mem(at::kLeader + 0x30)));
    const auto x = static_cast<short>(Word(Mem(at::kLeader + 0x2E)));
    const unsigned char list = B(at::kPartyList0);
    Sprite_Current = ObjTrio;
    const unsigned char effect = B(kinds + list);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 59's handlers 2 and 3 (0x40B360, 0x40B3B0): cells x 0x47..0x49 of row
// 8 set to `row8`, then of row 9 to `row9`.
void SetCells59(unsigned row8, unsigned row9) {
    for (unsigned x = 0x47; x <= 0x49; ++x) AH_CALL(AreaMap_SetByte)(x, 8, row8);
    for (unsigned x = 0x47; x <= 0x49; ++x) AH_CALL(AreaMap_SetByte)(x, 9, row9);
}

// ---- areas 63, 64 ----

// Areas 63 and 64's inits (0x40B720, 0x40B7F0): Rand & 0x3F walked through
// eight weights (the first whose weight exceeds what is left; none: 8);
// field objects 0..7 other than that one get byte +0 = 0; the one chosen (if
// any) is put at one of eight cells by Rand & 7 - x and z the pair's bytes
// << 16, its word +0x3E the ground's elevation there (x read back). Either
// way Field_EdgeBits = the leader's dword +0x134 less 5.
void PlaceOneObject(std::uint32_t positions, std::uint32_t weights) {
    auto left = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned pick = 0;
    for (; pick < 8; ++pick) {
        const unsigned char w = B(weights + pick);
        if (left < w) break;
        left = static_cast<unsigned char>(left - w);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != pick) Sprite_Objects[k * at::kObjectStride] = 0;
    if (pick < 8) {
        const unsigned r = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = Sprite_Objects + pick * at::kObjectStride;
        const std::int32_t z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2 + 1)) << 16);
        const std::int32_t x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2)) << 16);
        SetLong(object + 0x34, x);
        SetLong(object + 0x38, z);
        const long ground = AH_CALL(AreaMap_Elevation)(Long(object + 0x34), z);
        SetWord(object + 0x3E, static_cast<unsigned>(ground));
    }
    Field_EdgeBits = static_cast<unsigned short>(Long(Mem(at::kLeaderDword134)) - 5);
}

}  // namespace

// ===========================================================================
// Area 53 (descriptor 0x5FEA98): choice 0 (choice 1, handler 0 and the init
// are 0x437CC0, a shared `ret` of another block), and object trigger 42.
// ===========================================================================

// original 0x40AB00 (Area53_Choices[0]): message 0xFFFF; the focus object's
// dwords +0x18 and +0x1C = Area53_ChoicePairs[the s8 answer] bytes 0 and 1
// (the pointer and the answer read again for the second).
extern "C" void __cdecl Area53_ChoiceFocusPair(void) {
    unsigned char* focus = Focus();
    const unsigned char first = B(BySignedAnswer(at::kArea53ChoicePairs, 2, 0));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Focus();
    SetLong(focus + 0x1C, B(BySignedAnswer(at::kArea53ChoicePairs, 2, 1)));
}

// original 0x40AB40 (Field_ObjectTriggers id 42, (object, flags) ignored; in
// area 53's block by address): ScriptFlags_Set40, the mode tail kind 0x2C
// with state 0 and sub-kind 0xA. No answer of its own: eax is what
// ScriptFlags_Set40 left (Capcom's: Field_StatusBits | 0x40 in al), passed on.
extern "C" unsigned __cdecl Area53_Trigger42(unsigned char*, unsigned char*) {
    const unsigned left = area_harness::Call(reinterpret_cast<unsigned (__cdecl*)()>(&ScriptFlags_Set40))();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0xA;
    return left;
}

// ===========================================================================
// Area 55 (descriptor 0x6007D8): choices 0, 3 and 4 (1 and 2 are 0x420850 /
// 0x420870, another block's), object trigger 23. No handler, no init.
// ===========================================================================

// original 0x40AB60 (Area55_Choices[0]): answer 0 - with item (0, 8) held
// (Inventory_Count's word not 0) one taken (Inventory_Remove(0, 8, 1)) and
// message 0x22, else message 0x23 and the mark; any other answer message 0x24
// and the mark.
extern "C" void __cdecl Area55_ChoiceTakeItem8(void) {
    if (Answer() != 0) {
        MessageMarked(0x24);
        return;
    }
    if (static_cast<std::uint16_t>(AH_CALL(Inventory_Count)(0, 8, 0)) == 0) {
        MessageMarked(0x23);
        return;
    }
    AH_CALL(Inventory_Remove)(0, 8, 1);
    SetMessage(0x22);
}

// original 0x40ABC0 (Area55_Choices[3] and [4]): an answer: message 0x24 and
// the mark; 0: message 0xFFFF.
extern "C" void __cdecl Area55_ChoiceMark24(void) {
    if (Answer() != 0) {
        MessageMarked(0x24);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40ABF0 (Field_ObjectTriggers id 23, (object, flags) ignored):
// the mode tail kind 4 with sub-kind 2. Answers 0 in al.
extern "C" unsigned char __cdecl Area55_Trigger23(unsigned char*, unsigned char*) {
    Tail4(2);
    return 0;
}

// ===========================================================================
// Area 56 (descriptor 0x600BA0): sixteen choices, the last also handler 0
// (the fall through Area56_FallStates), object trigger 52. The init is
// 0x437CC0, a shared `ret`.
// ===========================================================================

// originals 0x40AC10..0x40AEB0 (Area56_Choices[0..6]): ChoicePoseFlag, the
// pose on answer 0 and the flag (8..0xE) on 1, but choices 3 and 5 the
// other way round.
extern "C" void __cdecl Area56_ChoicePoseFlag8(void) { ChoicePoseFlag(0, 1, 8); }
extern "C" void __cdecl Area56_ChoicePoseFlag9(void) { ChoicePoseFlag(0, 1, 9); }
extern "C" void __cdecl Area56_ChoicePoseFlagA(void) { ChoicePoseFlag(0, 1, 0xA); }
extern "C" void __cdecl Area56_ChoicePoseFlagB(void) { ChoicePoseFlag(1, 0, 0xB); }
extern "C" void __cdecl Area56_ChoicePoseFlagC(void) { ChoicePoseFlag(0, 1, 0xC); }
extern "C" void __cdecl Area56_ChoicePoseFlagD(void) { ChoicePoseFlag(1, 0, 0xD); }
extern "C" void __cdecl Area56_ChoicePoseFlagE(void) { ChoicePoseFlag(0, 1, 0xE); }

// originals 0x40AF20..0x40AFC0 (Area56_Choices[7..12]): message 9..0xE when
// the answer is 0 (choices 7..9), 2 (10) or 1 (11, 12); else 0xFFFF (the
// original's `neg` / `sbb` mask).
extern "C" void __cdecl Area56_ChoiceMessage9(void) { SetMessage(Answer() == 0 ? 9 : 0xFFFF); }
extern "C" void __cdecl Area56_ChoiceMessageA(void) { SetMessage(Answer() == 0 ? 0xA : 0xFFFF); }
extern "C" void __cdecl Area56_ChoiceMessageB(void) { SetMessage(Answer() == 0 ? 0xB : 0xFFFF); }
extern "C" void __cdecl Area56_ChoiceMessageC(void) { SetMessage(Answer() == 2 ? 0xC : 0xFFFF); }
extern "C" void __cdecl Area56_ChoiceMessageD(void) { SetMessage(Answer() == 1 ? 0xD : 0xFFFF); }
extern "C" void __cdecl Area56_ChoiceMessageE(void) { SetMessage(Answer() == 1 ? 0xE : 0xFFFF); }

// original 0x40AFE0 (Area56_Choices[13]): message 0xFFFF; answer 1:
// LeaderPose27.
extern "C" void __cdecl Area56_ChoicePoseOn1(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 1) LeaderPose27();
}

// original 0x40B020 (Area56_Choices[14]): message 0xFFFF; answer 0: sound
// 0x208, then with Cond_Flags row 3's flags 8..0xE all set (byte 0x903FA9 &
// 0x7F, read after the sound), LeaderPose27.
extern "C" void __cdecl Area56_ChoicePoseIfAllFlags(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(Sound_PlayEffect)(0x208);
    if ((B(at::kCondRow3 + 1) & 0x7F) != 0x7F) return;
    LeaderPose27();
}

// original 0x40B080 (Area56_Choices[15] = Area56_Handlers[0]; PSX
// 0x801F3284): Area56_FallStates by Sprite_Current[4] - Area56_FallBegin,
// Area56_FallStep, Area56_FallEnd; ours aborts past 3.
extern "C" void __cdecl Area56_FallRun(void) {
    StateEntry("Area56_FallRun", at::kArea56FallStates, at::kArea56FallStateCount, Sprite_Current[4])();
}

// original 0x40B0A0 (Area56_FallStates[0]): the object's bit 0x40 of +0 cleared;
// its height +0x3E the ground at its x, z plus 0x7D0; +0xC, +0x10, +0x14 0,
// the step +0x20 -8, state 1; Field_State's word +0x12E less 2.
extern "C" void __cdecl Area56_FallBegin(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    const unsigned char* const cur = Sprite_Current;
    const long ground = AH_CALL(MapView_GroundAt)(Long(cur + 0x34), Long(cur + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground + 0x7D0));
    SetLong(Sprite_Current + 0xC, 0);
    SetLong(Sprite_Current + 0x10, 0);
    SetLong(Sprite_Current + 0x14, 0);
    SetLong(Sprite_Current + 0x20, -8);
    Sprite_Current[4] = 1;
    FieldStatePace();
}

// original 0x40B120 (Area56_FallStates[1]): +0x14 += the step +0x20;
// Field_LeaderStepTick; the ground at the object's x, z above its height (a
// signed word compare): the height the ground, +0x14 and +0x20 0, state 2.
// Then with the object's +5 at 0 and bit 3 of neither Field_ScriptFlags2's
// nor Field_ScriptFlags' low byte, MapView_SetElevation(its height, a word).
// Field_State's word +0x12E less 2.
extern "C" void __cdecl Area56_FallStep(void) {
    unsigned char* cur = Sprite_Current;
    SetLong(cur + 0x14, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x14)) + static_cast<std::uint32_t>(Long(cur + 0x20))));
    AH_CALL(Field_LeaderStepTick)();
    cur = Sprite_Current;
    const long ground = AH_CALL(MapView_GroundAt)(Long(cur + 0x34), Long(cur + 0x38));
    if (static_cast<std::int16_t>(ground) > static_cast<std::int16_t>(Word(Sprite_Current + 0x3E))) {
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
        SetLong(Sprite_Current + 0x14, 0);
        SetLong(Sprite_Current + 0x20, 0);
        Sprite_Current[4] = 2;
    }
    cur = Sprite_Current;
    if (cur[5] == 0 && ((static_cast<unsigned char>(Field_ScriptFlags2) | static_cast<unsigned char>(Field_ScriptFlags)) & 8) == 0)
        AH_CALL(MapView_SetElevation)(static_cast<std::int16_t>(Word(cur + 0x3E)));
    FieldStatePace();
}

// original 0x40B1B0 (Area56_FallStates[2]): Field_ScriptFlags2 bit 0x200
// cleared, the object's state 0.
extern "C" void __cdecl Area56_FallEnd(void) {
    unsigned char* const cur = Sprite_Current;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFDFF);
    cur[4] = 0;
}

// original 0x40B1D0 (Field_ObjectTriggers id 52, (object, flags) ignored; in
// area 56's block by address): FlagsGather(0x6E).
extern "C" unsigned char __cdecl Area56_Trigger52(unsigned char*, unsigned char*) { return FlagsGather(0x6E); }

// ===========================================================================
// Area 57 (descriptor 0x602928): choice 0 (also handler 4), handlers 0..3,
// object trigger 53. No init.
// ===========================================================================

// original 0x40B210 (Area57_Choices[0] = Area57_Handlers[4]; PSX 0x801F2C04):
// the message word Area57_ChoiceMessages[the s8 answer] (unchecked).
extern "C" void __cdecl Area57_ChoiceMessage(void) { SetMessage(Word(Mem(BySignedAnswer(at::kArea57ChoiceMessages, 2, 0)))); }

// original 0x40B230 (Area57_Handlers[0]; PSX 0x801F2C30): SpawnAtLeader, kind 2.
extern "C" void __cdecl Area57_SpawnKind2AtLeader(void) { SpawnAtLeader(2, at::kArea57EffectKinds2); }

// original 0x40B280 (Area57_Handlers[1]; PSX 0x801F2CB0): SpawnAtLeader, kind 4.
extern "C" void __cdecl Area57_SpawnKind4AtLeader(void) { SpawnAtLeader(4, at::kArea57EffectKinds4); }

// original 0x40B2D0 (Area57_Handlers[2]; also area 39's handler 9, area 108's
// choice 16 / handler 13, area 145's choice 10 / handler 9; PSX 0x801F2D30):
// a 5-byte `jmp Sound_StopMusic`.
extern "C" void __cdecl Area57_StopMusic(void) { AH_CALL(Sound_StopMusic)(); }

// original 0x40B2E0 (Area57_Handlers[3]; also area 108's choice 17 / handler
// 14, area 145's choice 11 / handler 10; PSX 0x801F2D58): a 5-byte `jmp
// Sound_ResumeAll`.
extern "C" void __cdecl Area57_ResumeSound(void) { AH_CALL(Sound_ResumeAll)(); }

// original 0x40B2F0 (Field_ObjectTriggers id 53, (object, flags) ignored; in
// area 57's block by address): FlagsGather(0x6F).
extern "C" unsigned char __cdecl Area57_Trigger53(unsigned char*, unsigned char*) { return FlagsGather(0x6F); }

// ===========================================================================
// Area 59 (descriptor 0x603C98): choices 0, 3 and 4 (1 and 2 other blocks'),
// handlers 2 and 3 (0 and 1 other blocks'), the step hook, object trigger 24,
// its effect (Effect_KindHandlers entry 0xB6) and the effect's two states.
// ===========================================================================

// original 0x40B330 (Area59_Choices[0]): answer 0: message 2; else message 4
// and the mark.
extern "C" void __cdecl Area59_ChoiceMessage2or4(void) {
    if (Answer() == 0) {
        SetMessage(2);
        return;
    }
    MessageMarked(4);
}

// original 0x40B360 (Area59_Handlers[2]; PSX 0x801F2D90): cells x 0x47..0x49
// of rows 8 and 9 cleared.
extern "C" void __cdecl Area59_ClearCells(void) { SetCells59(0, 0); }

// original 0x40B3B0 (Area59_Handlers[3]; PSX 0x801F2E08): the same cells, row
// 8 0xC0 and row 9 0xA1.
extern "C" void __cdecl Area59_SetCells(void) { SetCells59(0xC0, 0xA1); }

// original 0x40B410 (Area_StepHook's case for area 59, `(x, z)` answering in
// al): with Cond_ByteFD 2 and story flag 0x33, a step whose high words fall
// in x 0x46..0x48 and z 0x29..0x2B while the leader's byte +8 is 0, 7 or 6:
// counter 0 = 0, Party_DropIn(0), answer 1. Otherwise 0. Area 36's hook with
// z 0x29 for 0x23.
extern "C" int __cdecl Area59_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    if (!AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x33)) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(x) >> 16) - 0x46) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x29) >= 3) return 0;
    const unsigned char pose = B(at::kLeaderPose);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(at::kCounter0) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x40B470 (Field_ObjectTriggers id 24, (object, flags) ignored):
// ScriptFlags_Set40; the focus object's +1 = 4, +0x83 = 0, word +0x8A = 0
// (the pointer read afresh each time); the mode tail kind 4 with sub-kind 5
// and counter 3 the focus object's index among Sprite_Objects (its offset
// divided by 0xA4, signed, truncated; the low byte). Answers 0 in al.
extern "C" unsigned char __cdecl Area59_Trigger24(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    Focus()[1] = 4;
    Focus()[0x83] = 0;
    SetWord(Focus() + 0x8A, 0);
    const auto offset = static_cast<std::int32_t>(Key(Focus()) - at::kSpriteObjects);
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 5;
    B(at::kCounter3) = static_cast<unsigned char>(offset / static_cast<std::int32_t>(at::kObjectStride));
    return 0;
}

// original 0x40B4D0 (Effect_KindHandlers entry 0xB6, the dword 0x655628; in
// area 59's block by address): Area59_EffectStates by Sprite_Current[1] -
// Area59_EffectGround, Area59_EffectStep; ours aborts past 2.
extern "C" void __cdecl Area59_EffectRun(void) {
    StateEntry("Area59_EffectRun", at::kArea59EffectStates, at::kArea59EffectStateCount, Sprite_Current[1])();
}

// original 0x40B4F0 (Area59_EffectStates[0]; also the first state of areas 36,
// 100, 112, 116, 143 and 146's effect tables): the object's +0x3C the ground
// at its x, z (a signed word) << 16, its state +1 one on (Sprite_Current read
// again after the call).
extern "C" void __cdecl Area59_EffectGround(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(Long(cur + 0x34), Long(cur + 0x38)));
    SetLong(Sprite_Current + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16));
    ++Sprite_Current[1];
}

// original 0x40B520 (Area59_EffectStates[1]): the object's position dwords
// +0x34, +0x38, +0x3C copied to the stack and handed to 0x4220D0 (area 36's
// Area36_EffectStep instruction for instruction).
extern "C" void __cdecl Area59_EffectStep(void) {
    const unsigned char* const cur = Sprite_Current;
    std::int32_t position[4] = {Long(cur + 0x34), Long(cur + 0x38), Long(cur + 0x3C), 0};
    AH_AT(void (__cdecl*)(std::int32_t*), at::kPositionHook)(position);
}

// ===========================================================================
// Area 60 (descriptor 0x603DE0): the init only.
// ===========================================================================

// original 0x40B550 (Area60 +0x40 init; PSX 0x801F3310): Field_ScriptFlags
// bit 0x2000 set (its high byte or'ed with 0x20).
extern "C" void __cdecl Area60_InitScriptFlag2000(void) {
    B(at::kScriptFlagsHigh) = static_cast<unsigned char>(B(at::kScriptFlagsHigh) | 0x20);
}

// ===========================================================================
// Area 61 (descriptor 0x6040B8): choices 0, 3 and 4 (1 and 2 other blocks'),
// object trigger 21.
// ===========================================================================

// original 0x40B560 (Area61_Choices[0]): answer 0: Party_Zenny 0 and message
// 2; else message 3 and the mark.
extern "C" void __cdecl Area61_ChoiceClearZenny(void) {
    if (Answer() != 0) {
        MessageMarked(3);
        return;
    }
    SetLong(Mem(at::kZenny), 0);
    SetMessage(2);
}

// original 0x40B590 (Area61_Choices[3] and [4]; also areas 59, 113 and 116's
// choices 3 and 4): an answer: message 4 and the mark; 0: message 0xFFFF.
extern "C" void __cdecl Area61_ChoiceMark4(void) {
    if (Answer() != 0) {
        MessageMarked(4);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40B5C0 (Field_ObjectTriggers id 21, (object, flags) ignored):
// the mode tail kind 4 with sub-kind 1. Answers 0 in al.
extern "C" unsigned char __cdecl Area61_Trigger21(unsigned char*, unsigned char*) {
    Tail4(1);
    return 0;
}

// ===========================================================================
// Area 62 (descriptor 0x6041E0): handler 0 arming the mode tail kind 0x2A.
// ===========================================================================

// original 0x40B5E0 (Area62_Handlers[0]; PSX 0x801F2C04): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 6: ScriptFlags_Set40, the
// mode tail kind 0x2A at state 0 and the active member's (read again) word
// +0x8A one on.
extern "C" void __cdecl Area62_ArmTailGiveItem(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(at::kLeaderByte89) != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    unsigned char* const member = Field_ActiveMember;
    B(at::kTailKind) = 0x2A;
    B(at::kTailState) = 0;
    AddWord(member + 0x8A, 1);
}

// original 0x40B620 (Field_ModeTailKinds[0x2A], armed by
// Area62_ArmTailGiveItem): by the s8 state. 0: counter 0 = 0xA, sound 0x10D,
// state 1. 1: once counter 0 is 0xB, item (0, 0x59)'s 16-byte name record to
// Text_Records, Inventory_Add(0, 0x59, 1) - added: sound 0x106,
// Msg_OpenSystem(2), story flag 0x68; not: Msg_OpenSystem(3) - then
// Field_Request 2 and state 2. 2: once Field_Request leaves 2,
// ScriptFlags_Clear40, counter 0, the tail kind and state 0. Any other state
// returns.
extern "C" void __cdecl Area62_TailGiveItem59(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        B(at::kCounter0) = 0xA;
        AH_CALL(Sound_PlayEffect)(0x10D);
        B(at::kTailState) = 1;
        return;
    case 1: {
        if (B(at::kCounter0) != 0xB) return;
        const unsigned char* const name = AH_CALL(Item_NamePtr)(0, 0x59);
        for (unsigned i = 0; i < 16; i += 4) SetLong(Mem(at::kTextRecords + i), Long(name + i));
        if (AH_CALL(Inventory_Add)(0, 0x59, 1) != 0) {
            AH_CALL(Sound_PlayEffect)(0x106);
            AH_CALL(Msg_OpenSystem)(2);
            AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x68);
        } else {
            AH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        B(at::kTailState) = 2;
        return;
    }
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kCounter0) = 0;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    default: return;
    }
}

// ===========================================================================
// Areas 63 and 64 (descriptors 0x6042B0, 0x604380; one PSX descriptor
// address, two overlays): the inits only.
// ===========================================================================

// original 0x40B720 (Area63 +0x40 init; PSX 0x801F2D2C): a 5-byte jmp over
// eleven nops, then PlaceOneObject with area 63's tables.
extern "C" void __cdecl Area63_InitPlaceObject(void) { PlaceOneObject(at::kArea63Positions, at::kArea63Weights); }

// original 0x40B7F0 (Area64 +0x40 init; PSX 0x801F2D2C): the same with area
// 64's tables (the same bytes today).
extern "C" void __cdecl Area64_InitPlaceObject(void) { PlaceOneObject(at::kArea64Positions, at::kArea64Weights); }

void AreaW1d_Inject() {
    if (bof3::WantsShadow("area_w1d")) area_w1d::SelfTest();
    BOF3_INJECT(Area53_ChoiceFocusPair);
    BOF3_INJECT(Area53_Trigger42);
    BOF3_INJECT(Area55_ChoiceTakeItem8);
    BOF3_INJECT(Area55_ChoiceMark24);
    BOF3_INJECT(Area55_Trigger23);
    BOF3_INJECT(Area56_ChoicePoseFlag8);
    BOF3_INJECT(Area56_ChoicePoseFlag9);
    BOF3_INJECT(Area56_ChoicePoseFlagA);
    BOF3_INJECT(Area56_ChoicePoseFlagB);
    BOF3_INJECT(Area56_ChoicePoseFlagC);
    BOF3_INJECT(Area56_ChoicePoseFlagD);
    BOF3_INJECT(Area56_ChoicePoseFlagE);
    BOF3_INJECT(Area56_ChoiceMessage9);
    BOF3_INJECT(Area56_ChoiceMessageA);
    BOF3_INJECT(Area56_ChoiceMessageB);
    BOF3_INJECT(Area56_ChoiceMessageC);
    BOF3_INJECT(Area56_ChoiceMessageD);
    BOF3_INJECT(Area56_ChoiceMessageE);
    BOF3_INJECT(Area56_ChoicePoseOn1);
    BOF3_INJECT(Area56_ChoicePoseIfAllFlags);
    BOF3_INJECT(Area56_FallRun);
    BOF3_INJECT(Area56_FallBegin);
    BOF3_INJECT(Area56_FallStep);
    BOF3_INJECT(Area56_FallEnd);
    BOF3_INJECT(Area56_Trigger52);
    BOF3_INJECT(Area57_ChoiceMessage);
    BOF3_INJECT(Area57_SpawnKind2AtLeader);
    BOF3_INJECT(Area57_SpawnKind4AtLeader);
    BOF3_INJECT(Area57_StopMusic);
    BOF3_INJECT(Area57_ResumeSound);
    BOF3_INJECT(Area57_Trigger53);
    BOF3_INJECT(Area59_ChoiceMessage2or4);
    BOF3_INJECT(Area59_ClearCells);
    BOF3_INJECT(Area59_SetCells);
    BOF3_INJECT(Area59_StepHook);
    BOF3_INJECT(Area59_Trigger24);
    BOF3_INJECT(Area59_EffectRun);
    BOF3_INJECT(Area59_EffectGround);
    BOF3_INJECT(Area59_EffectStep);
    BOF3_INJECT(Area60_InitScriptFlag2000);
    BOF3_INJECT(Area61_ChoiceClearZenny);
    BOF3_INJECT(Area61_ChoiceMark4);
    BOF3_INJECT(Area61_Trigger21);
    BOF3_INJECT(Area62_ArmTailGiveItem);
    BOF3_INJECT(Area62_TailGiveItem59);
    BOF3_INJECT(Area63_InitPlaceObject);
    BOF3_INJECT(Area64_InitPlaceObject);
}

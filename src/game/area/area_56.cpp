// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_56.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }

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
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Areas 56 and 57's object triggers 52 / 53 (0x40B1D0, 0x40B2F0): story flag
// `flag` set; when flags 0x6D..0x70 are all set, flag 0x71 too (area 34's
// trigger 51 is the same with 0x6D). Answers 0 in al.
unsigned char FlagsGather(unsigned flag) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
    for (unsigned f = 0x6D; f <= 0x70; ++f)
        if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), f)) return 0;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x71);
    return 0;
}

// The leader's state bytes +1 = 2, +2 = 7, +3 = 0, then MoveCmd_TestFB at
// the leader's cell (the high words of x and z, read before the stores; the
// answer is not read).
void LeaderPose27() {
    const std::uint16_t z = Word(Mem(::bof3::Img(ObjTrio_addr) + 0x3A));
    const std::uint16_t x = Word(Mem(::bof3::Img(ObjTrio_addr) + 0x36));
    B(::bof3::Img(ObjTrio_addr) + 1) = 2;
    B(::bof3::Img(ObjTrio_addr) + 2) = 7;
    B(::bof3::Img(ObjTrio_addr) + 3) = 0;
    AH_CALL(MoveCmd_TestFB)(static_cast<short>(x), static_cast<short>(z));
}

// Area 56's choices 0..6 (0x40AC10..0x40AEB0): message 0xFFFF; the answer at
// `pose_on`: LeaderPose27; the answer (read again) at `flag_on`: Cond_Flags
// row 3's flag `flag` set; the answer (read again) not 2: sound 0x208.
void ChoicePoseFlag(unsigned char pose_on, unsigned char flag_on, unsigned flag) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == pose_on) LeaderPose27();
    if (Answer() == flag_on) AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x18), flag);
    if (Answer() == 2) return;
    AH_CALL(Sound_PlayEffect)(0x208);
}

// Area 56's fall (Area56_FallStates): the running object's word +0x3E the
// height, +0x14 its rise, +0x20 the rise's step; each Sprite_Current read
// afresh, as the originals read it.
void FieldStatePace() { AddWord(Field_State + 0x12E, -2); }

}  // namespace

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
    if ((B(::bof3::Img(Cond_Flags_addr) + 0x18 + 1) & 0x7F) != 0x7F) return;
    LeaderPose27();
}

// original 0x40B080 (Area56_Choices[15] = Area56_Handlers[0]; PSX
// 0x801F3284): Area56_FallStates by Sprite_Current[4] - Area56_FallBegin,
// Area56_FallStep, Area56_FallEnd; ours aborts past 3.
extern "C" void __cdecl Area56_FallRun(void) {
    StateEntry("Area56_FallRun", ::bof3::Img(Area56_FallStates_addr), at::kArea56FallStateCount, Sprite_Current[4])();
}

// original 0x40B0A0 (Area56_FallStates[0]): the object's bit 0x40 of +0 cleared;
// its height +0x3E the ground at its x, z plus 0x7D0; +0xC, +0x10, +0x14 0,
// the step +0x20 -8, state 1; Field_State's word +0x12E less 2.
extern "C" void __cdecl Area56_FallBegin(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const long ground = AH_CALL(MapView_GroundAt)(tCur->x, tCur->z);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(ground + 0x7D0));
    rv::Current<records::SpriteObject>()->step_x = 0;
    rv::Current<records::SpriteObject>()->step_z = 0;
    rv::Current<records::SpriteObject>()->step_y = 0;
    rv::Store<std::int32_t>(rv::Current<records::SpriteObject>()->work_20, -8);
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
    auto* tCur = rv::As<records::SpriteObject>(cur);
    tCur->step_y = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->step_y) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(cur)->work_20));
    AH_CALL(Field_LeaderStepTick)();
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    const long ground = AH_CALL(MapView_GroundAt)(tCur->x, tCur->z);
    if (static_cast<std::int16_t>(ground) > static_cast<std::int16_t>(rv::Current<records::SpriteObject>()->height_whole)) {
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(ground));
        rv::Current<records::SpriteObject>()->step_y = 0;
        rv::Store<std::int32_t>(rv::Current<records::SpriteObject>()->work_20, 0);
        Sprite_Current[4] = 2;
    }
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    if (cur[5] == 0 && ((static_cast<unsigned char>(Field_ScriptFlags2) | static_cast<unsigned char>(Field_ScriptFlags)) & 8) == 0)
        AH_CALL(MapView_SetElevation)(static_cast<std::int16_t>(rv::As<records::SpriteObject>(cur)->height_whole));
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

void Area56_Inject() {
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
}

// Moved here by tools/move_funcs.py from area_w1c.cpp (docs/source-layout-round.md).
#include "game/area/area_49.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
}

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The four callees nobody owns, by their raw addresses (area_w1c_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// Area 49's handlers 8, 9, 10, 17 (0x409980, 0x4099D0, 0x409A20, 0x409B20):
// Effect_Spawn(kind, 0, Area49_EffectKinds[party list `member`'s first byte],
// party record `member`'s words +0x2E and +0x30) with Sprite_Current made
// that record (and left so); an answer other than 0xFF stored to
// Sprite_Current[0xB], read again after the call. The member byte indexes
// the table unchecked, as the original's.
void SpawnAtMember(unsigned member, unsigned char kind) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(::bof3::Img(Area49_EffectKinds_addr) + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 49's handlers 19 and 20 (0x409BD0, 0x409C10): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 6, ScriptFlags_Set40 and
// MoveScript_Var7 = 4 with step `step`.
void RunStep(unsigned char step) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x89);
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (pose != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = 4;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

// Area 49's effect frame: a member's bit in the effect's +0xB mask and in
// Field_ScriptFlags2 cleared, if it was set (the byte and the word, each as
// the original's; bits past 7 / 15 never set).
void ClearMemberBit(unsigned char* effect, unsigned bit) {
    if ((effect[0xB] & static_cast<unsigned char>(bit)) == 0) return;
    effect[0xB] = static_cast<unsigned char>(effect[0xB] & ~bit);
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit);
}

}  // namespace

// original 0x4093F0 (Area49_Choices[0]): the message word from
// Area49_ChoiceMessages[the answer as s8] (unchecked: a negative answer reads
// before it); answer 0 also sets flag 0x3B of the chapter's row (0x929ED0).
extern "C" void __cdecl Area49_ChoiceMessageFlag3B(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(Word(Mem(::bof3::Img(Area49_ChoiceMessages_addr) + static_cast<std::uint32_t>(answer * 2))));
    if (answer != 0) return;
    AH_CALL(Flags_Set)(FlagRow(), 0x3B);
}

// original 0x409420 (Area49_Choices[1]): the message word from
// Area49_ChoiceMessages[2 + the answer as s8], unchecked.
extern "C" void __cdecl Area49_ChoiceMessage(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(Word(Mem(::bof3::Img(Area49_ChoiceMessages_addr) + 4 + static_cast<std::uint32_t>(answer * 2))));
}

// original 0x409440 (Area_StepHook's case for area 49, `(x, z)` answering in
// al): a step at x exactly 0x4D8000 with Cond_ByteFD 0 runs
// Area49_SwapFlags2To3. Answers 0.
extern "C" int __cdecl Area49_StepHook(long x, long) {
    if (x == 0x4D8000 && Cond_ByteFD == 0) AH_CALL(Area49_SwapFlags2To3)();
    return 0;
}

// original 0x409460 (called by Area49_StepHook and Area49_EffectFrame):
// story flag 2 cleared, 3 set.
extern "C" void __cdecl Area49_SwapFlags2To3(void) {
    AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 2);
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 3);
}

// original 0x409480 (called only by effect kind 0x70's handler EffectKind70_Run 0x46D780 when
// Game_AreaNumber is 0x31; Sprite_Current the effect, its +0xB a mask of
// members). With Field_Request 5 and the area not the pending one 0x937F82:
// Area49_SwapFlags2To3 and done. Otherwise Field_ScriptFlags bit 0x2000
// cleared and, for each member m (the count read before, and again after each
// member), Sprite_Current and Field_State made party record m (Field_State
// left so; Sprite_Current put back at the end) and:
//  - Cond_Flags row 9's flag 0x25, or row 5's 0x3A, or m in no zone
//    (Area49_MemberZone 0xFF): m's bit cleared in the effect's mask and in
//    Field_ScriptFlags2, if it was set;
//  - in a zone, m's bit clear: unless m's bit is set in Field_ScriptFlags2
//    or Field_ScriptFlags, Field_State +0x128 = 2, the record's +9 = 0, the
//    bit set in both masks, the direction +8 from Area49_Zones[zone]'s byte 4
//    (turned by 4 when its story flag, byte 5, is set) and
//    Sprite_EnsureAnimation(+8);
//  - in a zone, m's bit set: a record with +9 at 0 runs Field_JumpSetUp (and
//    Field_JumpCamera for m 0); one mid-step turns to the zone's direction if
//    it differs (the steps +0xC, +0x10, +0x14 negated, +9 = 8 - +9); then
//    Field_LeaderStepTick, +9 less 1, Sprite_ScriptTick and Field_ScriptFlags
//    bit 0x2000 set.
// Then, with Field_Request 5, bit 0x2000 cleared again. Sprite_Current read
// again after every call, as the original's.
extern "C" void __cdecl Area49_EffectFrame(void) {
    if (Field_Request == 5 && Game_AreaNumber != Word(Mem(::bof3::Img(AreaW0b_Place_addr)))) {
        AH_CALL(Area49_SwapFlags2To3)();
        return;
    }
    const unsigned char count = Field_MemberCount;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    unsigned char* const effect = Sprite_Current;
    if (count != 0) {
        unsigned char m = 0;
        do {
            const unsigned bit = 1u << (m & 31);
            unsigned char* const record = PartyAt(m);
            Field_State = record;
            Sprite_Current = record;
            if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x48), 0x25)) {
                ClearMemberBit(effect, bit);
            } else if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x28), 0x3A)) {
                ClearMemberBit(effect, bit);
            } else {
                const unsigned char zone = AH_CALL(Area49_MemberZone)(m);
                const unsigned char* const row = Mem(::bof3::Img(Area49_Zones_addr) + zone * 6u);
                if (zone == 0xFF) {
                    ClearMemberBit(effect, bit);
                } else if ((effect[0xB] & static_cast<unsigned char>(bit)) == 0) {
                    if (((static_cast<unsigned>(Field_ScriptFlags2) | Field_ScriptFlags) & bit) == 0) {
                        Field_State[0x128] = 2;
                        Sprite_Current[9] = 0;
                        effect[0xB] = static_cast<unsigned char>(effect[0xB] | bit);
                        unsigned char* const cur = Sprite_Current;
                        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | bit);
                        cur[8] = row[4];
                        if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), row[5])) Sprite_Current[8] ^= 4;
                        AH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
                    }
                } else {
                    if (Sprite_Current[9] == 0) {
                        AH_CALL(Field_JumpSetUp)();
                        if (m == 0) AH_CALL(Field_JumpCamera)();
                    } else {
                        unsigned char dir = row[4];
                        if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), row[5])) dir ^= 4;
                        unsigned char* const cur = Sprite_Current;
                        if (cur[8] != dir) {
                            cur[8] = dir;
                            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_x, -rv::As<records::SpriteHead>(cur)->step_x);
                            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_z, -rv::As<records::SpriteHead>(cur)->step_z);
                            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_y, -rv::As<records::SpriteHead>(cur)->step_y);
                            cur[9] = static_cast<unsigned char>(8 - cur[9]);
                        }
                    }
                    AH_CALL(Field_LeaderStepTick)();
                    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
                    AH_CALL(Sprite_ScriptTick)();
                    B(::bof3::Img(at::kScriptFlagsHigh.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kScriptFlagsHigh.v)) | 0x20);
                }
            }
            ++m;
        } while (m < Field_MemberCount);
    }
    if (Field_Request == 5) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    Sprite_Current = effect;
}

// original 0x409760 (called by Area49_EffectFrame, `(member)`, answering in
// al): the first of Area49_Zones' seven rectangles holding party record
// member's (a byte) position +0x34 / +0x38 - x0 <= x <= x1 and z0 <= z <= z1,
// each bound the zone's byte << 16, signed compares; 0xFF for none.
extern "C" unsigned char __cdecl Area49_MemberZone(unsigned member) {
    const unsigned char* const record = PartyAt(member & 0xFF);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const std::int32_t x = tRecord->x, z = tRecord->z;
    for (unsigned i = 0; i < at::kArea49ZoneCount; ++i) {
        const unsigned char* const zone = Mem(::bof3::Img(Area49_Zones_addr) + i * 6u);
        if (x < static_cast<std::int32_t>(zone[0] << 16) || x > static_cast<std::int32_t>(zone[2] << 16)) continue;
        if (z < static_cast<std::int32_t>(zone[1] << 16) || z > static_cast<std::int32_t>(zone[3] << 16)) continue;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4097D0 (Area_CellHooks' entry for area 0x31, `(x, z)` answering
// in al): the first of Area49_CellSwitches' nine records whose x and z bytes
// are the cell's and whose pose is the leader's byte +8; none: 0. Story flag
// 0x1C set: 0. Else its story flag toggled (0x57C160), 0x469FE0(0xF),
// Sound_PlayEffect(0x202), answer 1.
extern "C" int __cdecl Area49_CellHook(long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    unsigned i = 0;
    for (; i < at::kArea49CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(::bof3::Img(Area49_CellSwitches_addr) + i * 4u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && r[2] == pose) break;
    }
    if (i == at::kArea49CellSwitchCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C)) return 0;
    FlagsToggle(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), B(::bof3::Img(Area49_CellSwitches_addr) + i * 4u + 3));
    AH_AT(void (__cdecl*)(unsigned), at::kSpawnKind4.v)(0xF);
    AH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// original 0x409850 (Area49_Choices[2] = Area49_Handlers[0]; PSX 0x801F3288):
// flag 0x3F of the chapter's row set.
extern "C" void __cdecl Area49_SetRowFlag3F(void) { AH_CALL(Flags_Set)(FlagRow(), 0x3F); }

// original 0x409870 (Area49_Handlers[1]; PSX 0x801F32B0): map cells x
// 0x29..0x2B of rows 0x5B and 0x5C zeroed.
extern "C" void __cdecl Area49_ClearCells(void) {
    for (unsigned z = 0x5B; z <= 0x5C; ++z)
        for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, z, 0);
}

// original 0x4098C0 (Area49_Handlers[2]; PSX 0x801F3328): the same cells set,
// row 0x5B to 0xC0 and row 0x5C to 0xA1.
extern "C" void __cdecl Area49_SetCells(void) {
    for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, 0x5B, 0xC0);
    for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, 0x5C, 0xA1);
}

// original 0x409920 (Area49_Handlers[3]; PSX 0x801F33A0): with the second
// party list's first byte 0x904063 at 4, counter 1 = 1.
extern "C" void __cdecl Area49_Counter1If4(void) {
    if (B(::bof3::Img(Cond_Flags_addr) + 0xd2 + 1) == 4) B(::bof3::Img(at::kCounter1.v)) = 1;
}

// original 0x409940 (Area49_Handlers[4]; PSX 0x801F33C4): with 0x904064 at 4,
// counter 2 = 1.
extern "C" void __cdecl Area49_Counter2If4(void) {
    if (B(::bof3::Img(Cond_Flags_addr) + 0xd2 + 2) == 4) B(::bof3::Img(at::kCounter2.v)) = 1;
}

// original 0x409960 (Area49_Handlers[5]; shared by areas 8, 15, 26, 49, 77,
// 80, 131; PSX 0x801F33E8 for area 49's): Camera_ShiftY + 0x14,
// MapView_Redraw 2.
extern "C" void __cdecl Area49_ShiftCameraUp(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x14);
    MapView_Redraw = 2;
}

// original 0x409970 (Area49_Handlers[6]; shared by areas 8, 15, 26, 49, 80,
// 131; PSX 0x801F3410): Camera_ShiftY - 0x14, MapView_Redraw 2.
extern "C" void __cdecl Area49_ShiftCameraDown(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x14);
    MapView_Redraw = 2;
}

// originals 0x409980, 0x4099D0, 0x409A20 (Area49_Handlers[8..10]; PSX
// 0x801F3454, 0x801F34D4, 0x801F3554): SpawnAtMember for members 0, 1, 2,
// effect kind 4.
extern "C" void __cdecl Area49_SpawnAtMember0(void) { SpawnAtMember(0, 4); }

extern "C" void __cdecl Area49_SpawnAtMember1(void) { SpawnAtMember(1, 4); }

extern "C" void __cdecl Area49_SpawnAtMember2(void) { SpawnAtMember(2, 4); }

// original 0x409A70 (Area49_Handlers[11]; PSX 0x801F35D4): map cells (0xB,
// 0x55) and (0xC, 0x55) zeroed.
extern "C" void __cdecl Area49_ClearCellsB(void) {
    SetByte(0xB, 0x55, 0);
    SetByte(0xC, 0x55, 0);
}

// original 0x409A90 (Area49_Handlers[12]; PSX 0x801F360C): with the leader's
// +0x89 at 5, an effect of kind 0x6C (Effect_FindFree; none: skipped),
// Sound_PlayEffect(0x209), the active member's +0x80 bit 0 cleared (the
// pointer read after the sound), cells (0xB, 0x55) and (0xC, 0x55) set to
// 0xC0, counter 0 = 0 and MoveScript_Var7 = 4 with step 0x1E. Otherwise only
// the active member's bit.
extern "C" void __cdecl Area49_SpawnEffect6C(void) {
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 5) {
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        return;
    }
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        unsigned char* const e = EffectAt(slot);
        e[0] = 1;
        e[5] = 0x6C;
    }
    AH_CALL(Sound_PlayEffect)(0x209);
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    SetByte(0xB, 0x55, 0xC0);
    SetByte(0xC, 0x55, 0xC0);
    B(::bof3::Img(at::kCounter0.v)) = 0;
    MoveScript_Var7 = 4;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x1E;
}

// original 0x409B20 (Area49_Handlers[17]; PSX 0x801F376C): SpawnAtMember for
// member 2, effect kind 3.
extern "C" void __cdecl Area49_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3); }

// original 0x409B70 (Area49_Handlers[18]; PSX 0x801F37EC): map cells x 0x4D
// and 0x4E of rows 0x22..0x25 zeroed.
extern "C" void __cdecl Area49_ClearCellsC(void) {
    for (unsigned x = 0x4D; x <= 0x4E; ++x)
        for (unsigned z = 0x22; z <= 0x25; ++z) SetByte(x, z, 0);
}

// originals 0x409BD0, 0x409C10 (Area49_Handlers[19], [20]; PSX 0x801F3884,
// 0x801F3908): RunStep with step 5, 6.
extern "C" void __cdecl Area49_RunStep5(void) { RunStep(5); }

extern "C" void __cdecl Area49_RunStep6(void) { RunStep(6); }

void Area49_Inject() {
    BOF3_INJECT(Area49_ChoiceMessageFlag3B);
    BOF3_INJECT(Area49_ChoiceMessage);
    BOF3_INJECT(Area49_StepHook);
    BOF3_INJECT(Area49_SwapFlags2To3);
    BOF3_INJECT(Area49_EffectFrame);
    BOF3_INJECT(Area49_MemberZone);
    BOF3_INJECT(Area49_CellHook);
    BOF3_INJECT(Area49_SetRowFlag3F);
    BOF3_INJECT(Area49_ClearCells);
    BOF3_INJECT(Area49_SetCells);
    BOF3_INJECT(Area49_Counter1If4);
    BOF3_INJECT(Area49_Counter2If4);
    BOF3_INJECT(Area49_ShiftCameraUp);
    BOF3_INJECT(Area49_ShiftCameraDown);
    BOF3_INJECT(Area49_SpawnAtMember0);
    BOF3_INJECT(Area49_SpawnAtMember1);
    BOF3_INJECT(Area49_SpawnAtMember2);
    BOF3_INJECT(Area49_ClearCellsB);
    BOF3_INJECT(Area49_SpawnEffect6C);
    BOF3_INJECT(Area49_SpawnKind3AtMember2);
    BOF3_INJECT(Area49_ClearCellsC);
    BOF3_INJECT(Area49_RunStep5);
    BOF3_INJECT(Area49_RunStep6);
}

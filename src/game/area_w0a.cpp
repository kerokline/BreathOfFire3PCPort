// World 0's first area overlays, 0x401000..0x401B80 (less area 11's three,
// area_011.cpp): the PSX's BIN/WORLD00/AREA000..015.EMI compiled into the
// exe, each area's handlers, choice handlers, init and state handlers, read
// to the last instruction with capstone (2026-09-27) and taken through the
// area harness (area_harness.h). docs/area_w0a.md has each area's
// descriptor, roots, block and tables.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Two
// dispatchers index a two-entry state table by an unchecked byte; ours aborts
// past the table where the original would jump into data (the project's rule
// for an index past its table, round9 doc section 6). Every call goes through
// the harness (AH_CALL), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies.
#include "game/area_w0a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(at::kCursor)[0]); }
unsigned char CursorByte() { return Mem(at::kCursor)[0]; }
void SetMessage(unsigned v) { SetWord(Mem(at::kMessage), v); }
// A u16 of an area's message table by the s8 cursor row, unchecked as the
// original's (a row outside the table reads its neighbours in the image).
unsigned MessageAt(std::uint32_t table, int row) {
    return Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + row * 2)));
}

// The index of an object from Sprite_Objects, as the original's magic
// multiply computes it: a signed 32-bit difference divided by 0xA4,
// truncated toward 0.
unsigned char ObjectIndex(const unsigned char* object) {
    const auto d = static_cast<std::int32_t>(Key(object) - area_harness::at::kObjects);
    return static_cast<unsigned char>(d / 0xA4);
}

unsigned char* Effect(std::uint32_t slot_dword) { return Mem(at::kEffects + (slot_dword & 0xFFu) * at::kEffectStride); }

// The script position of MoveScript_Object (u16 +0xA), moved as the
// original's 16-bit add.
void MoveScriptPos(unsigned delta) {
    unsigned char* const object = MoveScript_Object;
    SetWord(object + 0xA, Word(object + 0xA) + delta);
}

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

// The walk of area 13's handlers 2 and 3: the distance (wrapping 32-bit)
// made positive as cdq / xor / sub does (0x80000000 stays itself), divided
// by 0x8000 truncated toward 0, its low byte the step count at +7.
unsigned char Steps(std::uint32_t distance) {
    const auto d = static_cast<std::int32_t>(distance);
    const auto magnitude = static_cast<std::int32_t>(d < 0 ? 0u - distance : distance);
    return static_cast<unsigned char>(magnitude / 0x8000);
}

// Area 15's state dispatch: jmp [table + Sprite_Current[4] * 4], unchecked
// in the original; two entries, so ours aborts past them.
void RunState(std::uint32_t table, const char* who) {
    const unsigned state = Sprite_Current[4];
    if (state >= at::kArea15States)
        bof3::Fatal("%s: Sprite_Current[4] is %u, past the two-entry state table 0x%X (the original jumps into data)", who,
                    state, (unsigned)table);
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * state)))))();
}

// Area 15's first state of each table: the pose pair by frame parity, a
// countdown of Rand & 7, state 1, the script op repeated.
void Area15Pose(std::uint32_t poses) {
    const unsigned parity = (Frame_Counter >> 1) & 1u;
    const int r = AH_CALL(Rand)();
    Sprite_Current[0xA] = static_cast<unsigned char>(r & 7);
    AH_CALL(Sprite_EnsureAnimation)(Mem(poses + parity * 2)[0]);
    Sprite_Current[0x2A] = Mem(poses + parity * 2 + 1)[0];
    Sprite_Current[4] = 1;
    MoveScriptPos(0xFFFE);
}

}  // namespace

// ---- area 0 (descriptor Area0_Descriptor 0x5DB318) ---------------------------

// original 0x401000 (area 0's choice 0, Area00_Choices[0]; PSX 0x801F2C04):
// no new message; row 0 sets variables 3 and 6 to 0x14, row 1 both to 0x19.
extern "C" void __cdecl Area00_ChoiceVars3And6(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) {
        Mem(at::kVar3)[0] = 0x14;
        Mem(at::kVar6)[0] = 0x14;
    } else if (row == 1) {
        Mem(at::kVar3)[0] = 0x19;
        Mem(at::kVar6)[0] = 0x19;
    }
}

// original 0x401040 (area 0's choice 1; PSX 0x801F2C4C): no new message; row
// 0 sets variable 3 to 0x14, row 1 to 0x19.
extern "C" void __cdecl Area00_ChoiceVar3(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(at::kVar3)[0] = 0x14;
    else if (row == 1) Mem(at::kVar3)[0] = 0x19;
}

// original 0x401070 (area 0's handler 0 and choice 2; PSX 0x801F2C8C):
// Sprite_Current[8] = Field_ActiveMember[0x85], then
// Sprite_FaceDirection(Sprite_Current[8]), Sprite_Current read again.
extern "C" void __cdecl Area00_FaceAsActiveMember(void) {
    Sprite_Current[8] = Field_ActiveMember[0x85];
    AH_CALL(Sprite_FaceDirection)(Sprite_Current[8]);
}

// original 0x4010A0 (area 0's handler 1 and choice 3; PSX 0x801F2CD8):
// MoveCmd_Move(MoveScript_Object, Sprite_Current[8]), then the script
// object's +7 = 2, MoveScript_Object read again.
extern "C" void __cdecl Area00_MoveFacing(void) {
    AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
    MoveScript_Object[7] = 2;
}

// ---- area 1 (descriptor 0x5DC760) --------------------------------------------

// original 0x4010D0 (area 1's handler 0 and choice 2; PSX 0x801F2C88):
// variable 6 = the index of Field_ActiveMember from Sprite_Objects.
extern "C" void __cdecl Area01_ActiveMemberToVar6(void) {
    Mem(at::kVar6)[0] = ObjectIndex(Field_ActiveMember);
}

// original 0x401100 (area 1's handler 1 and choice 3; PSX 0x801F2CD4): with
// variable 5 above 6 (unsigned) and Frame_Counter's low three bits 0, a free
// effect record of kind 0xB: +0 1, +5 0xB, the dword +0x1C 5, +7 3. The slot
// is kept at 0x903850 and read back from it.
extern "C" void __cdecl Area01_SpawnEffectB(void) {
    if (Mem(at::kVar5)[0] <= 6) return;
    if ((Frame_Counter & 7u) != 0) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Mem(at::kEffectSlot)[0] = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(static_cast<std::uint32_t>(Long(Mem(at::kEffectSlot))));
    e[0] = 1;
    e[5] = 0xB;
    SetLong(e + 0x1C, 5);
    e[7] = 3;
}

// ---- area 2 (descriptor 0x5DD5C8) --------------------------------------------

// original 0x401150 (area 2's choice 0): no new message; row 0 sets the byte
// 0x9039F5 to 5, row 1 to 4, another row nothing more; then tail kind 5 and
// story flag 0x1E.
extern "C" void __cdecl Area02_ChoiceArmTail(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(at::kTailArg)[0] = 5;
    else if (row == 1) Mem(at::kTailArg)[0] = 4;
    else return;
    Mem(at::kTailKind)[0] = 5;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x1E);
}

// ---- area 3 (descriptor 0x5DDB40) --------------------------------------------

// original 0x401190 (area 3's choice 0 and handler 2; PSX 0x801F2C04): row 0
// opens message 0x42; another row message 0x44 and the byte 0x9398CF 6.
extern "C" void __cdecl Area03_ChoiceMessage42(void) {
    if (CursorByte() == 0) {
        SetMessage(0x42);
        return;
    }
    SetMessage(0x44);
    Mem(at::kByte9398CF)[0] = 6;
}

// original 0x4011C0 (area 3's choices 3 and 4, handlers 5 and 6; PSX
// 0x801F2CA8): a row other than 0 opens message 0x44 and sets the byte
// 0x9398CF 6; row 0 no new message.
extern "C" void __cdecl Area03_ChoiceMessage44(void) {
    if (CursorByte() != 0) {
        SetMessage(0x44);
        Mem(at::kByte9398CF)[0] = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x4011F0 (area 3's handler 1; PSX 0x801F2D88): four map cells,
// (0x55 and 0x54, 0x1C and 0x1D), set 0xC0 and 0xA1.
extern "C" void __cdecl Area03_SetCells(void) {
    static const unsigned char kCells[][3] = {{0x55, 0x1C, 0xC0}, {0x55, 0x1D, 0xC0}, {0x54, 0x1C, 0xA1}, {0x54, 0x1D, 0xA1}};
    SetCells(kCells, 4);
}

// original 0x401230 (Field_ObjectTriggers entry 22, called by 0x56E020 with
// the object and 0x904030, both unread; no area's descriptor names it, it
// lies in area 3's block): ScriptFlags_Set40, tail kind 4 with the byte
// 0x9039F5 0, and al 0 (eax above it ScriptFlags_Set40's leftover).
extern "C" unsigned char __cdecl Area03_Trigger22(unsigned char* object, unsigned char* flags) {
    static_cast<void>(object);
    static_cast<void>(flags);
    AH_CALL(ScriptFlags_Set40)();
    Mem(at::kTailKind)[0] = 4;
    Mem(at::kTailArg)[0] = 0;
    return 0;
}

// ---- area 4 (descriptor 0x5DE8A0) --------------------------------------------

// original 0x401250 (area 4's choice 0): no new message; row 0 sets the byte
// 0x9039F5 to 0, row 1 to 1, another row nothing more; then tail kind 5 and
// story flag 0x1F.
extern "C" void __cdecl Area04_ChoiceArmTail(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(at::kTailArg)[0] = 0;
    else if (row == 1) Mem(at::kTailArg)[0] = 1;
    else return;
    Mem(at::kTailKind)[0] = 5;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x1F);
}

// ---- area 5 (descriptor 0x5DF6A0) --------------------------------------------

// original 0x401290 (area 5's choice 2): the message Area05_MessagesA[row].
extern "C" void __cdecl Area05_ChoiceMessageA(void) { SetMessage(MessageAt(at::kArea05MessagesA, CursorRow())); }

// original 0x4012B0 (area 5's choice 3): the message Area05_MessagesB[row].
extern "C" void __cdecl Area05_ChoiceMessageB(void) { SetMessage(MessageAt(at::kArea05MessagesB, CursorRow())); }

// original 0x401670 (area 5's choice 0, area 10's choice 0, area 15's choice
// 0 and handler 6; PSX 0x801F2CB8 as area 15's handler 6): no new message;
// row 0 sets variable 3 to 0x64, row 1 to 0x65.
extern "C" void __cdecl Area05_ChoiceVar3At64(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(at::kVar3)[0] = 0x64;
    else if (row == 1) Mem(at::kVar3)[0] = 0x65;
}

// original 0x4016A0 (area 5's choice 1, area 10's choice 1): no new message;
// row 0 sets variable 3 to 0xC8, row 1 to 0xC9.
extern "C" void __cdecl Area05_ChoiceVar3AtC8(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(at::kVar3)[0] = 0xC8;
    else if (row == 1) Mem(at::kVar3)[0] = 0xC9;
}

// ---- area 7 (descriptor 0x5E0378) --------------------------------------------

// original 0x4012D0 / 0x4012E0 / 0x4012F0 (area 7's handlers 1..3, area 19's
// 2..4; PSX 0x801F2C24 / 0x801F2C44 / 0x801F2C64 as area 7's): Kind2_Place
// with 1, 2, 3.
extern "C" void __cdecl Area07_PlaceKind2At1(void) { AH_CALL(Kind2_Place)(1); }
extern "C" void __cdecl Area07_PlaceKind2At2(void) { AH_CALL(Kind2_Place)(2); }
extern "C" void __cdecl Area07_PlaceKind2At3(void) { AH_CALL(Kind2_Place)(3); }

// original 0x401300 (area 7's handler 4; PSX 0x801F2C84): 38 map cells set,
// in this order - 0 on twelve, 0x50 on the twelve beside them, 0 on fourteen
// more.
extern "C" void __cdecl Area07_SetCells(void) {
    static const unsigned char kCells[][3] = {
        {0x12, 0x5, 0x0},   {0x12, 0x6, 0x0},   {0x12, 0xC, 0x0},   {0x12, 0xD, 0x0},   {0xE, 0x21, 0x0},
        {0xE, 0x22, 0x0},   {0x1B, 0x16, 0x0},  {0x1C, 0x16, 0x0},  {0x20, 0x16, 0x0},  {0x21, 0x16, 0x0},
        {0x20, 0x1E, 0x0},  {0x20, 0x1F, 0x0},  {0x11, 0x5, 0x50},  {0x11, 0x6, 0x50},  {0x11, 0xC, 0x50},
        {0x11, 0xD, 0x50},  {0xD, 0x21, 0x50},  {0xD, 0x22, 0x50},  {0x1B, 0x15, 0x50}, {0x1C, 0x15, 0x50},
        {0x20, 0x15, 0x50}, {0x21, 0x15, 0x50}, {0x1F, 0x1E, 0x50}, {0x1F, 0x1F, 0x50}, {0x1, 0x15, 0x0},
        {0x1, 0x14, 0x0},   {0x1, 0x13, 0x0},   {0x2, 0x15, 0x0},   {0x2, 0x14, 0x0},   {0x2, 0x13, 0x0},
        {0x15, 0x2, 0x0},   {0x16, 0x2, 0x0},   {0x15, 0x1, 0x0},   {0x16, 0x1, 0x0},   {0x26, 0x18, 0x0},
        {0x26, 0x19, 0x0},  {0x27, 0x18, 0x0},  {0x27, 0x19, 0x0},
    };
    SetCells(kCells, 38);
}

// original 0x4014C0 / 0x4014D0 (area 7's handlers 6 / 7, area 131's 0 / 1;
// PSX 0x801F2F20 / 0x801F2F48 as area 7's): Camera_ShiftY 10 less / more
// (a 16-bit add), MapView_Redraw 2.
extern "C" void __cdecl Area07_CameraShiftYLess(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0xA);
    MapView_Redraw = 2;
}
extern "C" void __cdecl Area07_CameraShiftYMore(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0xA);
    MapView_Redraw = 2;
}

// ---- area 8 (descriptor 0x5E1078) --------------------------------------------

// original 0x4014E0 (area 8's choice 0; PSX 0x801F42EC): the message
// Area08_ChoiceMessages[row]; row 0 sets variable 3 to 1, row 1 to 0.
extern "C" void __cdecl Area08_ChoiceMessageVar3(void) {
    const int row = CursorRow();
    SetMessage(MessageAt(at::kArea08ChoiceMessages, row));
    if (row == 0) Mem(at::kVar3)[0] = 1;
    else if (row == 1) Mem(at::kVar3)[0] = 0;
}

// original 0x401510 (area 8's handler 0 and choice 1; PSX 0x801F434C): three
// map cells set.
extern "C" void __cdecl Area08_SetCells(void) {
    static const unsigned char kCells[][3] = {{0x33, 0x15, 0x0}, {0x33, 0x16, 0x0}, {0x32, 0x16, 0x51}};
    SetCells(kCells, 3);
}

// original 0x401540 / 0x401570 (area 8's handlers 1 / 2 and choices 2 / 3;
// PSX 0x801F4394 / 0x801F43E0): Crt_sprintf(Text_Records,
// Area08_MessageFormat, variable 6 & 0x7F), then Msg_OpenScript 0x54 / 0x56
// and Field_Request 2; the second then Music_Play(0xA, 8).
extern "C" void __cdecl Area08_OpenMessage54(void) {
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(at::kTextRecords)),
                         reinterpret_cast<const char*>(Mem(at::kArea08MessageFormat)), Mem(at::kVar6)[0] & 0x7F);
    AH_CALL(Msg_OpenScript)(0x54);
    Field_Request = 2;
}
extern "C" void __cdecl Area08_OpenMessage56(void) {
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(at::kTextRecords)),
                         reinterpret_cast<const char*>(Mem(at::kArea08MessageFormat)), Mem(at::kVar6)[0] & 0x7F);
    AH_CALL(Msg_OpenScript)(0x56);
    Field_Request = 2;
    AH_CALL(Music_Play)(0xA, 8);
}

// original 0x4015B0 (area 8's handler 3 and choice 4; PSX 0x801F443C): a
// free effect record of kind 0x54 (+0 1, +5 0x54) whose +0xB is the index of
// Field_ActiveMember from Sprite_Objects, read after the call. The slot is
// kept at 0x903850 and read back from it.
extern "C" void __cdecl Area08_SpawnEffect54(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Mem(at::kEffectSlot)[0] = slot;
    if (slot == 0xFF) return;
    const unsigned char member = ObjectIndex(Field_ActiveMember);
    unsigned char* const e = Effect(static_cast<std::uint32_t>(Long(Mem(at::kEffectSlot))));
    e[0] = 1;
    e[5] = 0x54;
    e[0xB] = member;
}

// original 0x401600 / 0x401610 (area 8's handlers 4 / 5; PSX 0x801F4524 /
// 0x801F454C): Camera_ShiftX 0x14 more / less (a 16-bit add), MapView_Redraw 2.
extern "C" void __cdecl Area08_CameraShiftXMore(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX + 0x14);
    MapView_Redraw = 2;
}
extern "C" void __cdecl Area08_CameraShiftXLess(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX - 0x14);
    MapView_Redraw = 2;
}

// original 0x401620 / 0x401630 (area 8's handlers 10 / 11; PSX 0x801F45FC /
// 0x801F4618): field object 6's x (the dword Sprite_Objects + 6 * 0xA4 +
// 0x34) 0x800 more / less.
extern "C" void __cdecl Area08_Object6XMore(void) { SetLong(Mem(at::kObject6X), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(at::kObject6X))) + 0x800u)); }
extern "C" void __cdecl Area08_Object6XLess(void) { SetLong(Mem(at::kObject6X), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(at::kObject6X))) - 0x800u)); }

// original 0x401640 (area 8's handler 12 and choice 13, area 3's handler 0;
// PSX 0x801F4634 as area 8's, 0x801F2D30 as area 3's): the four cells
// Area03_SetCells sets, cleared.
extern "C" void __cdecl Area08_ClearCells(void) {
    static const unsigned char kCells[][3] = {{0x55, 0x1C, 0x0}, {0x55, 0x1D, 0x0}, {0x54, 0x1C, 0x0}, {0x54, 0x1D, 0x0}};
    SetCells(kCells, 4);
}

// ---- area 10 (descriptor 0x5E23A8) -------------------------------------------

// original 0x4016D0 / 0x4016F0 (area 10's choices 2 / 3): the message
// Area10_MessagesA / B[row].
extern "C" void __cdecl Area10_ChoiceMessageA(void) { SetMessage(MessageAt(at::kArea10MessagesA, CursorRow())); }
extern "C" void __cdecl Area10_ChoiceMessageB(void) { SetMessage(MessageAt(at::kArea10MessagesB, CursorRow())); }

// original 0x401710 (area 10's handler 0 and choice 4; PSX 0x801F2CDC): a
// free effect record of kind 0x1C (+0 1, +5 0x1C) at Sprite_Current's x and
// z (the dwords +0x34 and +0x38, Sprite_Current read for each).
extern "C" void __cdecl Area10_SpawnEffect1C(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = 0x1C;
    SetLong(e + 0x34, Long(Sprite_Current + 0x34));
    SetLong(e + 0x38, Long(Sprite_Current + 0x38));
}

// ---- area 12 (descriptor 0x5E2AA8) -------------------------------------------

// original 0x401840 (area 12's handler 0; PSX 0x801F2C04): until flag 0xE of
// the bank at [0x929ED0] is set, the script position 2 back (the op runs
// again), MoveScript_Object read after the call.
extern "C" void __cdecl Area12_WaitFlagE(void) {
    const auto bank = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kFlagBank)))));
    if (AH_CALL(Flags_Test)(bank, 0xE)) return;
    MoveScriptPos(0xFFFE);
}

// ---- area 13 (descriptor 0x5E3FB8) -------------------------------------------

// original 0x401860 (area 13's choice 0; PSX 0x801F2C04): the message
// Area13_ChoiceMessages[row]; row 0 sets variable 3 to 0xA, row 1 to 0x14.
extern "C" void __cdecl Area13_ChoiceMessageVar3(void) {
    const int row = CursorRow();
    SetMessage(MessageAt(at::kArea13ChoiceMessages, row));
    if (row == 0) Mem(at::kVar3)[0] = 0xA;
    else if (row == 1) Mem(at::kVar3)[0] = 0x14;
}

// original 0x401890 (area 13's handler 0 and choice 1; PSX 0x801F2C54): four
// map cells, (0x39 and 0x3A, 3 and 2), cleared.
extern "C" void __cdecl Area13_ClearCells(void) {
    static const unsigned char kCells[][3] = {{0x39, 0x3, 0x0}, {0x3A, 0x3, 0x0}, {0x39, 0x2, 0x0}, {0x3A, 0x2, 0x0}};
    SetCells(kCells, 4);
}

// original 0x4018C0 (area 13's handler 1 and choice 2; PSX 0x801F2CAC):
// unless the leader's byte +0x89 is 8, the script position 0xC on.
extern "C" void __cdecl Area13_SkipUnlessLeader8(void) {
    if (Field_State[0x89] == 8) return;
    MoveScriptPos(0xC);
}

// original 0x4018E0 (area 13's handler 2 and choice 3; PSX 0x801F2CEC): the
// object walks along x to 0x1C8000: direction 3 when 0x1C8000 - x is not
// negative (32-bit, wrapping), else 7; the step count +7 of MoveScript_Object
// the distance / 0x8000; with steps, MoveCmd_Move(MoveScript_Object,
// Sprite_Current[8]).
extern "C" void __cdecl Area13_WalkToX(void) {
    unsigned char* const object = Sprite_Current;
    const std::uint32_t distance = 0x1C8000u - static_cast<std::uint32_t>(Long(object + 0x34));
    object[8] = static_cast<std::int32_t>(distance) >= 0 ? 3 : 7;
    MoveScript_Object[7] = Steps(distance);
    unsigned char* const script = MoveScript_Object;
    if (script[7] == 0) return;
    AH_CALL(MoveCmd_Move)(script, Sprite_Current[8]);
}

// original 0x401940 (area 13's handler 3 and choice 4; PSX 0x801F2D88): the
// object walks along z to 0x20000 in direction 1; the step count +7 the
// distance / 0x8000; with steps, MoveCmd_Move(MoveScript_Object,
// Sprite_Current[8]).
extern "C" void __cdecl Area13_WalkToZ(void) {
    unsigned char* const object = Sprite_Current;
    const std::uint32_t z = static_cast<std::uint32_t>(Long(object + 0x38));
    object[8] = 1;
    MoveScript_Object[7] = Steps(0x20000u - z);
    unsigned char* const script = MoveScript_Object;
    if (script[7] == 0) return;
    AH_CALL(MoveCmd_Move)(script, Sprite_Current[8]);
}

// original 0x401990 (area 13's handler 4 and choice 5; PSX 0x801F2E18):
// Sprite_Current[8] one direction on, of eight.
extern "C" void __cdecl Area13_TurnDirection(void) {
    Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 1) & 7);
}

// ---- area 15 (descriptor 0x5E5B80) -------------------------------------------

// original 0x4019B0 (area 15's init, the descriptor's +0x40; PSX 0x801F2C04):
// the zone byte (+4) of the area's zone records 1 and 2 (the list at
// [0x668D80 + Game_AreaNumber * 4], read for each) is 2 with bit 0 of
// Field_StatusBits (MoveScript_Variable's variable 9), else 0.
extern "C" void __cdecl Area15_SetZones(void) {
    const unsigned char zone = (Field_StatusBits & 1) ? 2 : 0;
    auto list = [] {
        return Mem(static_cast<std::uint32_t>(Long(Mem(at::kZoneLists + static_cast<std::uint32_t>(Game_AreaNumber) * 4u))));
    };
    list()[0xC] = zone;
    list()[0x14] = zone;
}

// original 0x401A00 (area 15's handler 0; PSX 0x801F2CF8): Draw_PassFlags 0x1F.
extern "C" void __cdecl Area15_PassFlags1F(void) { Draw_PassFlags = 0x1F; }

// original 0x401A10 / 0x401AF0 (area 15's handlers 4 / 5; PSX 0x801F2D78 /
// 0x801F2EE8): jmp [Area15_StatesA / B + Sprite_Current[4] * 4].
extern "C" void __cdecl Area15_RunStatesA(void) { RunState(at::kArea15StatesA, "Area15_RunStatesA"); }
extern "C" void __cdecl Area15_RunStatesB(void) { RunState(at::kArea15StatesB, "Area15_RunStatesB"); }

// original 0x401A30 / 0x401B10 (Area15_StatesA / B entry 0): the pose of
// Area15_PosesA / B by bit 1 of Frame_Counter (read before Rand), a
// countdown Sprite_Current[0xA] = Rand & 7, state 1, the script position 2
// back. Sprite_Current read after each call, MoveScript_Object at the end.
extern "C" void __cdecl Area15_StateA0(void) { Area15Pose(at::kArea15PosesA); }
extern "C" void __cdecl Area15_StateB0(void) { Area15Pose(at::kArea15PosesB); }

// original 0x401AA0 (entry 1 of both state tables): the countdown
// Sprite_Current[0xA] one less; at 0 before it, state 0. Then with the
// scene's timer 0x8034E6 at 0, state 0 and the countdown 0; else the script
// position 2 back.
extern "C" void __cdecl Area15_StateCount(void) {
    const unsigned char count = Sprite_Current[0xA];
    Sprite_Current[0xA] = static_cast<unsigned char>(count - 1);
    if (count == 0) Sprite_Current[4] = 0;
    if (Word(Mem(at::kTimer)) == 0) {
        Sprite_Current[4] = 0;
        Sprite_Current[0xA] = 0;
        return;
    }
    MoveScriptPos(0xFFFE);
}

void AreaW0a_Inject() {
    if (bof3::WantsShadow("area_w0a")) area_w0a::SelfTest();
    BOF3_INJECT(Area00_ChoiceVars3And6);
    BOF3_INJECT(Area00_ChoiceVar3);
    BOF3_INJECT(Area00_FaceAsActiveMember);
    BOF3_INJECT(Area00_MoveFacing);
    BOF3_INJECT(Area01_ActiveMemberToVar6);
    BOF3_INJECT(Area01_SpawnEffectB);
    BOF3_INJECT(Area02_ChoiceArmTail);
    BOF3_INJECT(Area03_ChoiceMessage42);
    BOF3_INJECT(Area03_ChoiceMessage44);
    BOF3_INJECT(Area03_SetCells);
    BOF3_INJECT(Area03_Trigger22);
    BOF3_INJECT(Area04_ChoiceArmTail);
    BOF3_INJECT(Area05_ChoiceMessageA);
    BOF3_INJECT(Area05_ChoiceMessageB);
    BOF3_INJECT(Area07_PlaceKind2At1);
    BOF3_INJECT(Area07_PlaceKind2At2);
    BOF3_INJECT(Area07_PlaceKind2At3);
    BOF3_INJECT(Area07_SetCells);
    BOF3_INJECT(Area07_CameraShiftYLess);
    BOF3_INJECT(Area07_CameraShiftYMore);
    BOF3_INJECT(Area08_ChoiceMessageVar3);
    BOF3_INJECT(Area08_SetCells);
    BOF3_INJECT(Area08_OpenMessage54);
    BOF3_INJECT(Area08_OpenMessage56);
    BOF3_INJECT(Area08_SpawnEffect54);
    BOF3_INJECT(Area08_CameraShiftXMore);
    BOF3_INJECT(Area08_CameraShiftXLess);
    BOF3_INJECT(Area08_Object6XMore);
    BOF3_INJECT(Area08_Object6XLess);
    BOF3_INJECT(Area08_ClearCells);
    BOF3_INJECT(Area05_ChoiceVar3At64);
    BOF3_INJECT(Area05_ChoiceVar3AtC8);
    BOF3_INJECT(Area10_ChoiceMessageA);
    BOF3_INJECT(Area10_ChoiceMessageB);
    BOF3_INJECT(Area10_SpawnEffect1C);
    BOF3_INJECT(Area12_WaitFlagE);
    BOF3_INJECT(Area13_ChoiceMessageVar3);
    BOF3_INJECT(Area13_ClearCells);
    BOF3_INJECT(Area13_SkipUnlessLeader8);
    BOF3_INJECT(Area13_WalkToX);
    BOF3_INJECT(Area13_WalkToZ);
    BOF3_INJECT(Area13_TurnDirection);
    BOF3_INJECT(Area15_SetZones);
    BOF3_INJECT(Area15_PassFlags1F);
    BOF3_INJECT(Area15_RunStatesA);
    BOF3_INJECT(Area15_StateA0);
    BOF3_INJECT(Area15_StateCount);
    BOF3_INJECT(Area15_RunStatesB);
    BOF3_INJECT(Area15_StateB0);
}

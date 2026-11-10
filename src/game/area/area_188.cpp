// Moved here by tools/move_funcs.py from area_w4e.cpp (docs/source-layout-round.md).
#include "game/area/area_188.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4e::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* Cur() { return Sprite_Current; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The movement script's position (MoveScript_Object's word +0xA) back by 2.
void ScriptBack2() { AddWord(MoveScript_Object + 0xA, -2); }

// The tail armed from a choice: ScriptFlags_Set40, then kind and state (the
// sub-kind left as it is).
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The tail disarmed: kind, state and sub-kind 0 (the order of the stores
// around ScriptFlags_Clear40 is each caller's).
void ClearTail3() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
}

// Areas 188 and 191's choice that stores a byte pair at the focus object
// (0x42A3E0's first half, 0x42B570): the answer read as s8 indexes the pairs
// (unchecked: a negative answer reads before the table, in .data); +0x18 the
// first byte, +0x1C the second, both dwords; the focus pointer and the answer
// read again between; the message word 0xFFFF.
void FocusPair(U pairs) {
    const auto a = static_cast<signed char>(Answer());
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned char first = B(pairs + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const auto a2 = static_cast<signed char>(Answer());
    SetLong(focus + 0x1C, B(pairs + 1 + static_cast<U>(a2 * 2)));
}

// Area 188's map patches (handlers 4..8): AreaMap_SetByte(x, z, value) in
// turn.
struct Cell { unsigned char x, z, value; };

void SetCells(const Cell* cells, unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i].x, cells[i].z, cells[i].value);
}

}  // namespace

// original 0x42A320 (area 188 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 43 (Area188_Tail43) at state 0, sub-kind 0.
extern "C" void __cdecl Area188_ChoiceTail43A(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    ArmTail(0x2B, 0);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
}

// original 0x42A350 (area 188 +0x34[1]): the same with sub-kind 1.
extern "C" void __cdecl Area188_ChoiceTail43B(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    ArmTail(0x2B, 0);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 1;
}

// original 0x42A380 (area 188 +0x34[2]): message 0xFFFF. Cond_ByteFA (s8)
// above 12: answer 0 arms tail kind 10 (engine, 0x56DB80) at state 0,
// sub-kind 0xFF, without ScriptFlags_Set40. Else by the answer (s8): 0 -
// ScriptFlags_Set40, counter 0 = 0xA; 1 - ScriptFlags_Set40, counter 0 = 5.
extern "C" void __cdecl Area188_ChoiceByChapter(void) {
    const signed char chapter = Cond_ByteFA;
    SetMessage(0xFFFF);
    if (chapter > 12) {
        if (Answer() != 0) return;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xA;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xFF;
        return;
    }
    switch (static_cast<signed char>(Answer())) {
    case 0:
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(at::kCounter0.v)) = 0xA;
        return;
    case 1:
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(at::kCounter0.v)) = 5;
        return;
    default: return;
    }
}

// original 0x42A3E0 (area 188 +0x34[3]): FocusPair over Area188_FocusPairs;
// then with Cond_ByteFA (s8) at most 12 and the answer (s8) 1: the byte
// 0x929F0F 0, ScriptFlags_Set40, the run: step 0x14, MoveScript_Var7 8.
extern "C" void __cdecl Area188_ChoiceFocusPair(void) {
    FocusPair(::bof3::Img(Area188_FocusPairs_addr));
    if (Cond_ByteFA > 12) return;
    if (static_cast<signed char>(Answer()) != 1) return;
    B(::bof3::Img(AreaW3c_Load0F_addr)) = 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x14;
    MoveScript_Var7 = 8;
}

// original 0x42A450 (area 188 +0x3C[0] = +0x34[4]; PSX 0x801F4840): the first
// dword of the engine's direction record (0x6697B4, 8 bytes each) for the
// running object's facing (+8 & 7), plus its z (+0x38), 32-bit; unsigned at
// most Area188_ZLimits[the tail's sub-kind] << 16 (the byte unchecked): the
// script object's +7 = 1, MoveCmd_Move(the script object, 5), the script back
// 2 (MoveScript_Object read again for each).
extern "C" void __cdecl Area188_WalkWhileZUnder(void) {
    const unsigned char* const cur = Cur();
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const U v = static_cast<U>(Long(Mem(::bof3::Img(Field_DirectionSteps_addr) + 0x4 + (cur[8] & 7u) * 8))) + static_cast<U>(tCur->z);
    const U limit = static_cast<U>(B(::bof3::Img(Area188_ZLimits_addr) + B(::bof3::Img(AreaW0c_TailSub_addr)))) << 16;
    if (v > limit) return;
    MoveScript_Object[7] = 1;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    ScriptBack2();
}

// original 0x42A4B0 (area 188 +0x3C[2] = +0x34[6]; area 119 +0x34[3] =
// +0x3C[2], area 131 +0x3C[6], area 136 +0x34[13] = +0x3C[9]; PSX
// 0x801F4908): Camera_ShiftY - 2, MapView_Redraw 2 (Area136_ShiftCameraUp2's
// mirror).
extern "C" void __cdecl Area188_ShiftCameraDown2(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 2);
    MapView_Redraw = 2;
}

// original 0x42A4C0 (area 188 +0x3C[4] = +0x34[8]; PSX 0x801F494C): the cells
// (0xF, 0x3C) and (0x10, 0x3C) to 0, (0xF, 0x3B) and (0x10, 0x3B) to 0x51.
extern "C" void __cdecl Area188_PatchCellsA0(void) {
    static const Cell kCells[] = {{0xF, 0x3C, 0}, {0x10, 0x3C, 0}, {0xF, 0x3B, 0x51}, {0x10, 0x3B, 0x51}};
    SetCells(kCells, 4);
}

// original 0x42A4F0 (area 188 +0x3C[5] = +0x34[9]; PSX 0x801F49A4): the same
// cells to 0xC0, 0xC0, 0xA1, 0xA1.
extern "C" void __cdecl Area188_PatchCellsA1(void) {
    static const Cell kCells[] = {{0xF, 0x3C, 0xC0}, {0x10, 0x3C, 0xC0}, {0xF, 0x3B, 0xA1}, {0x10, 0x3B, 0xA1}};
    SetCells(kCells, 4);
}

// original 0x42A530 (area 188 +0x3C[6] = +0x34[10]; PSX 0x801F49FC): (0x1C, 3)
// and (0x1D, 3) to 0, (0x1F, 0x15) and (0x1F, 0x16) to 0x50.
extern "C" void __cdecl Area188_PatchCellsB0(void) {
    static const Cell kCells[] = {{0x1C, 3, 0}, {0x1D, 3, 0}, {0x1F, 0x15, 0x50}, {0x1F, 0x16, 0x50}};
    SetCells(kCells, 4);
}

// original 0x42A560 (area 188 +0x3C[7] = +0x34[11]; PSX 0x801F4A54): (0x1C, 3)
// and (0x1D, 3) to 0xC0, (0x20, 0x15) and (0x20, 0x16) to 0xA1.
extern "C" void __cdecl Area188_PatchCellsB1(void) {
    static const Cell kCells[] = {{0x1C, 3, 0xC0}, {0x1D, 3, 0xC0}, {0x20, 0x15, 0xA1}, {0x20, 0x16, 0xA1}};
    SetCells(kCells, 4);
}

// original 0x42A5A0 (area 188 +0x3C[8] = +0x34[12]; PSX 0x801F4AAC): six cells
// to 0: (0x38, 0x47), (0x38, 0x48), (0x39, 0x47), (0x39, 0x48), (0x37, 0x3F),
// (0x37, 0x40).
extern "C" void __cdecl Area188_ClearCellsC(void) {
    static const Cell kCells[] = {{0x38, 0x47, 0}, {0x38, 0x48, 0}, {0x39, 0x47, 0}, {0x39, 0x48, 0}, {0x37, 0x3F, 0}, {0x37, 0x40, 0}};
    SetCells(kCells, 6);
}

// original 0x42A5F0 (area 188 +0x3C[9] = +0x34[13]; PSX 0x801F4B24): an effect
// slot from Effect_FindFree; not 0xFF: its record's +0 = 1 and +5 = 0xB9
// (effect kind 0xB9).
extern "C" void __cdecl Area188_SpawnEffectB9(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const record = rv::Bytes(&Effect_Objects[slot]);
    record[0] = 1;
    record[5] = 0xB9;
}

// original 0x42A620 (Field_ModeTailKinds[43]): by the s8 state (a byte table
// of 14 at +0x16C into a jump table of 8 at +0x14C, both inside the extent;
// the ja bounds it unsigned, so a negative state does nothing): 0 - state 0xA
// with key item 6, else 1. 1 - once Field_Request is not 2: message 0x83,
// request 2, state 2. 2 - once not 2: kind, state, sub-kind 0 and
// ScriptFlags_Clear40. 0xA - once not 2: message 0x82, request 2, story flag
// 0x69 toggled, state 0xB. 0xB - once not 2: Party_DropIn(6), state 0xC. 0xC -
// once counter 3 is 0x28: Field_ChangeArea(0xBC, 0x330000, 0x470000, 0x87)
// for sub-kind 0, else (0xBC, 0x80000, 0x220000, 0x88); state 0xD. 0xD - once
// counter 3 is 0: ScriptFlags_Clear40, kind, state, sub-kind 0. 3..9 and past
// 0xD: nothing.
extern "C" void __cdecl Area188_Tail43(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        B(::bof3::Img(AreaW0c_TailState_addr)) = AH_CALL(KeyItem_Has)(6) != 0 ? 0xA : 1;
        return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x83);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2:
        if (Field_Request == 2) return;
        ClearTail3();
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 0xA:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x82);
        Field_Request = 2;
        AH_CALL(Flags_Toggle)(StoryFlags(), 0x69);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        AH_CALL(Party_DropIn)(6);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        return;
    case 0xC:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x28) return;
        if (B(::bof3::Img(AreaW0c_TailSub_addr)) == 0) AH_CALL(Field_ChangeArea)(0xBC, 0x330000, 0x470000, 0x87);
        else AH_CALL(Field_ChangeArea)(0xBC, 0x80000, 0x220000, 0x88);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xD;
        return;
    case 0xD:
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        ClearTail3();
        return;
    default: return;
    }
}

// original 0x42A7A0 (area 188 +0x40; PSX 0x801F4D88): the byte 0x905E68 1 with
// Cond_ByteFD 4: Sprite_ObjectsExtra record 0's +0x83 = 0xC; 0x905E68 4 with
// Cond_ByteFD 1: = 0xE.
extern "C" void __cdecl Area188_Init(void) {
    const unsigned char mode = B(::bof3::Img(AreaW1a_EntryZone_addr));
    if (mode == 1) {
        if (Cond_ByteFD == 4) B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 0xC;
        return;
    }
    if (mode == 4 && Cond_ByteFD == 1) B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 0xE;
}

void Area188_Inject() {
    BOF3_INJECT(Area188_ChoiceTail43A);
    BOF3_INJECT(Area188_ChoiceTail43B);
    BOF3_INJECT(Area188_ChoiceByChapter);
    BOF3_INJECT(Area188_ChoiceFocusPair);
    BOF3_INJECT(Area188_WalkWhileZUnder);
    BOF3_INJECT(Area188_ShiftCameraDown2);
    BOF3_INJECT(Area188_PatchCellsA0);
    BOF3_INJECT(Area188_PatchCellsA1);
    BOF3_INJECT(Area188_PatchCellsB0);
    BOF3_INJECT(Area188_PatchCellsB1);
    BOF3_INJECT(Area188_ClearCellsC);
    BOF3_INJECT(Area188_SpawnEffectB9);
    BOF3_INJECT(Area188_Tail43);
    BOF3_INJECT(Area188_Init);
}

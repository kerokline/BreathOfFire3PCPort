// World 4's areas 188..191: the PSX's BIN/WORLD04/AREA188..191.EMI compiled
// into the exe at 0x42A320..0x42BD60 (Area_Descriptors entries 188..191; 190
// has no code). Round ten, group AR4E: the band's 51 functions, none ours
// before, each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). docs/area_w4e.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 189's five leader states and area 191's three
// scale states abort past their tables (what follows is data, which the
// original would jump into), and area 189's step divides by a byte the
// original never lets be 0 but does not check (ours aborts there); every other
// unchecked read stays inside mapped memory and is kept (docs/area_w4e.md
// section 6). Every call goes through the harness (AH_CALL / AH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// the group's own callees are called the same way, so each function is fuzzed
// alone.
#include "game/area_w4e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4e::at;
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
unsigned char* Cur() { return Sprite_Current; }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char Answer() { return B(at::kChoiceAnswer); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
unsigned char* Row14() { return Mem(at::kCondRow14); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(at::kFlagRow); }
bool Test(const unsigned char* bits, unsigned index) { return AH_CALL(Flags_Test)(bits, index) != 0; }
// The movement script's position (MoveScript_Object's word +0xA) back by 2.
void ScriptBack2() { AddWord(MoveScript_Object + 0xA, -2); }
// The low word of Cond_AngleFB, the only part any store here writes.
void SetAngleFB(unsigned v) { SetWord(reinterpret_cast<unsigned char*>(&Cond_AngleFB), v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count) bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed from a choice: ScriptFlags_Set40, then kind and state (the
// sub-kind left as it is).
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = kind;
    B(at::kTailState) = state;
}
// The tail disarmed: kind, state and sub-kind 0 (the order of the stores
// around ScriptFlags_Clear40 is each caller's).
void ClearTail3() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0;
}
// A run the script starts: MoveScript_Var7 and the step byte after it.
void SetRun(signed char var7, unsigned char step) {
    MoveScript_Var7 = var7;
    B(at::kVar7Step) = step;
}

// Areas 188 and 191's choice that stores a byte pair at the focus object
// (0x42A3E0's first half, 0x42B570): the answer read as s8 indexes the pairs
// (unchecked: a negative answer reads before the table, in .data); +0x18 the
// first byte, +0x1C the second, both dwords; the focus pointer and the answer
// read again between; the message word 0xFFFF.
void FocusPair(U pairs) {
    const auto a = static_cast<signed char>(Answer());
    unsigned char* focus = Ptr(at::kFocusObject);
    const unsigned char first = B(pairs + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Ptr(at::kFocusObject);
    const auto a2 = static_cast<signed char>(Answer());
    SetLong(focus + 0x1C, B(pairs + 1 + static_cast<U>(a2 * 2)));
}

// Area 188's map patches (handlers 4..8): AreaMap_SetByte(x, z, value) in
// turn.
struct Cell { unsigned char x, z, value; };
void SetCells(const Cell* cells, unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i].x, cells[i].z, cells[i].value);
}

// fld / fstp of a float: the bits copied, but a signalling NaN quieted (bit
// 22 set) as the x87 does. Kept as the instructions themselves.
void CopyFloatX87(unsigned char* to, const unsigned char* from) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(to), "r"(from) : "memory");
}

// 0x511C10 (x, z): the map's height, in ax.
using HeightFn = short (__cdecl*)(long, long);
short HeightAt(U x, U z) { return AH_AT(HeightFn, at::kHeightAt)(static_cast<long>(x), static_cast<long>(z)); }

// Area 189: the pending area change of its walk and exit button:
// Field_ChangeArea(the area word, x, z, the flags byte) from the four cells.
void ChangeToPending() {
    AH_CALL(Field_ChangeArea)(Word(Mem(at::kPlace)), Long(Mem(at::kPendingX)), Long(Mem(at::kPendingZ)), B(at::kChangeFlags));
}
// The walk's facing for the next area: Area189_FacingJitter[Rand() & 3] +
// the leader's +8, the low nibble (Sprite_Current read after Rand).
void JitterFacing() {
    const unsigned r = static_cast<unsigned>(AH_CALL(Rand)()) & 3;
    const unsigned char* const cur = Cur();
    B(at::kWalkFacing) = static_cast<unsigned char>((B(at::kArea189FacingJitter + r) + cur[8]) & 0xF);
}
// A 16.16 coordinate's high word with its low byte cleared, signed (the
// walk's compares: `and al, 0` on the word, then a signed 16-bit cmp).
std::int16_t Block(U v) { return static_cast<std::int16_t>((v >> 16) & 0xFF00); }
std::int16_t BlockWord(const unsigned char* p) { return static_cast<std::int16_t>(Word(p) & 0xFF00); }

}  // namespace

// ===========================================================================
// Area 188 (descriptor 0x6474E0; PSX 0x801F6848)
// ===========================================================================

// original 0x42A320 (area 188 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 43 (Area188_Tail43) at state 0, sub-kind 0.
extern "C" void __cdecl Area188_ChoiceTail43A(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    ArmTail(0x2B, 0);
    B(at::kTailSub) = 0;
}

// original 0x42A350 (area 188 +0x34[1]): the same with sub-kind 1.
extern "C" void __cdecl Area188_ChoiceTail43B(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    ArmTail(0x2B, 0);
    B(at::kTailSub) = 1;
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
        B(at::kTailKind) = 0xA;
        B(at::kTailState) = 0;
        B(at::kTailSub) = 0xFF;
        return;
    }
    switch (static_cast<signed char>(Answer())) {
    case 0:
        AH_CALL(ScriptFlags_Set40)();
        B(at::kCounter0) = 0xA;
        return;
    case 1:
        AH_CALL(ScriptFlags_Set40)();
        B(at::kCounter0) = 5;
        return;
    default: return;
    }
}

// original 0x42A3E0 (area 188 +0x34[3]): FocusPair over Area188_FocusPairs;
// then with Cond_ByteFA (s8) at most 12 and the answer (s8) 1: the byte
// 0x929F0F 0, ScriptFlags_Set40, the run: step 0x14, MoveScript_Var7 8.
extern "C" void __cdecl Area188_ChoiceFocusPair(void) {
    FocusPair(at::kArea188FocusPairs);
    if (Cond_ByteFA > 12) return;
    if (static_cast<signed char>(Answer()) != 1) return;
    B(at::k929F0F) = 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kVar7Step) = 0x14;
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
    const U v = static_cast<U>(Long(Mem(at::kDirections + (cur[8] & 7u) * 8))) + static_cast<U>(Long(cur + 0x38));
    const U limit = static_cast<U>(B(at::kArea188ZLimits + B(at::kTailSub))) << 16;
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
    unsigned char* const record = Effect_Objects + slot * at::kEffectStride;
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
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        B(at::kTailState) = AH_CALL(KeyItem_Has)(6) != 0 ? 0xA : 1;
        return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x83);
        Field_Request = 2;
        B(at::kTailState) = 2;
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
        B(at::kTailState) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        AH_CALL(Party_DropIn)(6);
        B(at::kTailState) = 0xC;
        return;
    case 0xC:
        if (B(at::kCounter3) != 0x28) return;
        if (B(at::kTailSub) == 0) AH_CALL(Field_ChangeArea)(0xBC, 0x330000, 0x470000, 0x87);
        else AH_CALL(Field_ChangeArea)(0xBC, 0x80000, 0x220000, 0x88);
        B(at::kTailState) = 0xD;
        return;
    case 0xD:
        if (B(at::kCounter3) != 0) return;
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
    const unsigned char mode = B(at::kMode905E68);
    if (mode == 1) {
        if (Cond_ByteFD == 4) B(at::kExtra0Byte83) = 0xC;
        return;
    }
    if (mode == 4 && Cond_ByteFD == 1) B(at::kExtra0Byte83) = 0xE;
}

// ===========================================================================
// Area 189 (descriptor 0x647558; PSX 0x801F62AC)
// ===========================================================================

// original 0x42A7D0 (area 189 +0x40; PSX 0x801F4C38): story flag 0x82 set:
// Draw_PassFlags 0x1F.
extern "C" void __cdecl Area189_Init(void) {
    if (Test(StoryFlags(), 0x82)) Draw_PassFlags = 0x1F;
}

// original 0x42A7F0 (area 189 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 50 (Area189_Tail50) at state 0.
extern "C" void __cdecl Area189_ChoiceTail50A(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 0) ArmTail(0x32, 0);
}

// original 0x42A820 (area 189 +0x34[1]): the same at state 1.
extern "C" void __cdecl Area189_ChoiceTail50B(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 0) ArmTail(0x32, 1);
}

// original 0x42A850 (Field_ModeTailKinds[50]): once Field_Request is not 2:
// ScriptFlags_Clear40; Field_StatusBits bit 0 cleared (read after the call,
// with the state); state not 0: Field_ChangeArea(0x96, 0x180000, 0x300000, 1),
// else (0xC1, 0x1A8000, 0x1C0000, 1); kind and state 0.
extern "C" void __cdecl Area189_Tail50(void) {
    if (Field_Request == 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    const unsigned char bits = Field_StatusBits;
    const unsigned char state = B(at::kTailState);
    Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
    if (state != 0) AH_CALL(Field_ChangeArea)(0x96, 0x180000, 0x300000, 1);
    else AH_CALL(Field_ChangeArea)(0xC1, 0x1A8000, 0x1C0000, 1);
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// original 0x42A8B0 (Field_LeaderStates[13], 0x66094C): Area189_LeaderStates
// by Sprite_Current +2 (unchecked there; ours aborts past its five).
extern "C" void __cdecl Area189_LeaderRun(void) {
    StateEntry("Area189_LeaderRun", at::kArea189LeaderStates, at::kArea189LeaderStateCount, Cur()[2])();
}

// original 0x42A8D0 (Area189_LeaderStates 0): the walk set up.
// Gte_SetGeomOffset(0xA0, 0xB9), Gte_SetGeomScreen(0x12C); Camera_Angles[0]
// 0xFD12; the leader's facing +8 = the byte 0x904153; Camera_Distance 0xF280;
// MapView_Redraw 4; Cond_AngleFB's low word ((2 - that byte, read again) &
// 0xF) << 8; Field_Kind2X / Z the leader's x / z; +0x3C the height at (x, z)
// (0x511C10) << 16; Area189_ZeroSpeeds; +0x24, +0x4B 0; Field_EdgeBits 0.
// The pace: (Rand() & 0x1F) + 0x96, doubled when any of Party_Count(0)'s
// members answers Actor_EquipCount(member, 3, 0x15), halved (arithmetic)
// when any answers (member, 3, 0x14), doubled again with Cond_ByteFF 0; the
// word 0x802E74 = the pace. Story flags 0x75, 0x76, 0x8B..0x8E cleared; +2 + 1.
extern "C" void __cdecl Area189_LeaderStart(void) {
    AH_CALL(Gte_SetGeomOffset)(0xA0, 0xB9);
    AH_CALL(Gte_SetGeomScreen)(0x12C);
    unsigned char* cur = Cur();
    const unsigned char facing = B(at::kWalkFacing);
    Camera_Angles[0] = static_cast<short>(0xFD12);
    cur[8] = facing;
    const unsigned char again = B(at::kWalkFacing);
    Camera_Distance = static_cast<short>(0xF280);
    MapView_Redraw = 4;
    SetAngleFB(((2u - again) & 0xF) << 8);
    cur = Cur();
    Field_Kind2X = Long(cur + 0x34);
    Field_Kind2Z = Long(cur + 0x38);
    const short height = HeightAt(static_cast<U>(Long(cur + 0x34)), static_cast<U>(Long(cur + 0x38)));
    SetLong(Cur() + 0x3C, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(height)) << 16));
    AH_CALL(Area189_ZeroSpeeds)();
    Cur()[0x24] = 0;
    Cur()[0x4B] = 0;
    Field_EdgeBits = 0;
    std::int32_t pace = static_cast<std::int32_t>((static_cast<U>(AH_CALL(Rand)()) & 0x1F) + 0x96);
    const unsigned count = static_cast<unsigned>(AH_CALL(Party_Count)(0)) & 0xFF;
    for (unsigned i = 0; i < count; ++i) {
        if (AH_CALL(Actor_EquipCount)(Field_Members[i * 0x14C], 3, 0x15) != 0) {
            pace <<= 1;
            break;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        if (AH_CALL(Actor_EquipCount)(Field_Members[i * 0x14C], 3, 0x14) != 0) {
            pace >>= 1;
            break;
        }
    }
    if (Cond_ByteFF == 0) pace <<= 1;
    SetWord(Mem(at::kLeaderPace), static_cast<unsigned>(pace));
    AH_CALL(Flags_Clear)(StoryFlags(), 0x75);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x76);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8B);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8C);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8D);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8E);
    cur = Cur();
    cur[2] = static_cast<unsigned char>(cur[2] + 1);
}

// original 0x42AA80 (Area189_LeaderStates 1): Prim_VertexScratch = (x, z, y)
// of the kind-2 point: Field_Kind2X and Z each ((v - 1) & 0xFFFFFF) + 1, >> 9,
// - 0x4000, and -(the leader's s16 +0x3E / 2, toward 0); Gte_RotTransPers of
// it to MapView_ScreenXY; Gte_StoreDepthF(+0x7C); +0x78 = MapView_ScreenXY's
// y (through the x87); +0x14 = 1. Cond_Flags row 14 bit 9 set and bit 0xA
// clear: Effect_Objects[0] bit 6, +2 = 3, +0x10 = -0x2000, +9 = 0x78, the
// word 0x8034E6 = 0x3C. Else +2 = 2.
extern "C" void __cdecl Area189_LeaderProject(void) {
    auto scaled = [](U v) { return static_cast<short>((static_cast<std::int32_t>(((v - 1u) & 0xFFFFFFu) + 1u) >> 9) - 0x4000); };
    Prim_VertexScratch[0] = scaled(static_cast<U>(Field_Kind2X));
    Prim_VertexScratch[1] = scaled(static_cast<U>(Field_Kind2Z));
    const auto y = static_cast<std::int32_t>(static_cast<std::int16_t>(Word(Cur() + 0x3E)));
    Prim_VertexScratch[2] = static_cast<short>(-(y / 2));
    long depth[2];
    AH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth[1]);
    AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(Cur() + 0x7C));
    CopyFloatX87(Cur() + 0x78, Mem(at::kScreenY));
    SetLong(Cur() + 0x14, 1);
    if (Test(Row14(), 9) && !Test(Row14(), 0xA)) {
        Effect_Objects[0] = static_cast<unsigned char>(Effect_Objects[0] | 0x40);
        unsigned char* const cur = Cur();
        cur[2] = 3;
        SetLong(cur + 0x10, -0x2000);
        cur[9] = 0x78;
        SetWord(Mem(at::kWord8034E6), 0x3C);
        return;
    }
    Cur()[2] = 2;
}

// original 0x42AB90 (Area189_LeaderStates 2; also the tail jump of
// Area189_StepArrive): nothing under Field_ScriptFlags bit 8,
// Field_ScriptFlags2 bit 6 or Field_Request, or while Area189_ExitButton or
// Area189_MenuButton answers. Area189_TurnInput: 0 - +9 = 0 and
// Area189_LeaderHalt; 1 - Field_State +0x137 = 1, Cond_AngleFB's low word =
// (it + the s8 +0xB) & 0xFFF, +2 = 4; 2 - Area189_StepBegin, then the point
// +9 steps on (x + +0xC * +9, z + +0x10 * +9, 32-bit): its blocks (the high
// words with the low byte cleared, signed) x 0x1000..0x1400 with z from
// 0x1800 - message 2, request 2, the halt; else with Cond_Flags row 14 bit 4,
// x 0x1500..0x1900 and z at most 0xE00 - message 3, request 2, the halt;
// else +9 - 1, Area189_StepMove, Field_State +0x137 = 1, +2 = 3. Any other
// answer: nothing.
extern "C" void __cdecl Area189_LeaderControl(void) {
    if ((Field_ScriptFlags & 0x100) != 0) return;
    if ((Field_ScriptFlags2 & 0x40) != 0) return;
    if (Field_Request != 0) return;
    if (AH_CALL(Area189_ExitButton)() != 0) return;
    if (AH_CALL(Area189_MenuButton)() != 0) return;
    switch (AH_CALL(Area189_TurnInput)()) {
    case 0:
        Cur()[9] = 0;
        AH_CALL(Area189_LeaderHalt)();
        return;
    case 1: {
        Field_State[0x137] = 1;
        unsigned char* const cur = Cur();
        SetAngleFB((static_cast<U>(Cond_AngleFB) + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur[0xB])))) & 0xFFF);
        cur[2] = 4;
        return;
    }
    case 2: break;
    default: return;
    }
    AH_CALL(Area189_StepBegin)();
    const unsigned char* const cur = Cur();
    const U n = cur[9];
    const std::int16_t bx = Block(static_cast<U>(Long(cur + 0xC)) * n + static_cast<U>(Long(cur + 0x34)));
    const std::int16_t bz = Block(static_cast<U>(Long(cur + 0x10)) * n + static_cast<U>(Long(cur + 0x38)));
    if (bx >= 0x1000 && bx <= 0x1400 && bz >= 0x1800) {
        AH_CALL(Msg_OpenScript)(2);
        Field_Request = 2;
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if (Test(Row14(), 4) && bx >= 0x1500 && bx <= 0x1900 && bz <= 0xE00) {
        AH_CALL(Msg_OpenScript)(3);
        Field_Request = 2;
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Area189_StepMove)();
    Field_State[0x137] = 1;
    Cur()[2] = 3;
}

// original 0x42AD30 (Area189_LeaderStates 3): Field_State +0x137 = 1; +9 at
// 0: a tail jump to Area189_StepArrive; else +9 - 1 and a tail jump to
// Area189_StepMove.
extern "C" void __cdecl Area189_LeaderStep(void) {
    Field_State[0x137] = 1;
    unsigned char* const cur = Cur();
    const unsigned char left = cur[9];
    if (left == 0) {
        AH_CALL(Area189_StepArrive)();
        return;
    }
    cur[9] = static_cast<unsigned char>(left - 1);
    AH_CALL(Area189_StepMove)();
}

// original 0x42AD60 (Area189_LeaderStates 4): Cond_AngleFB's low word = (it +
// the s8 +0xB) & 0xFFF; equal to ((2 - +8) & 0xF) << 8: Field_State +0x137 =
// 0, +2 = 2.
extern "C" void __cdecl Area189_LeaderTurn(void) {
    unsigned char* const cur = Cur();
    const U angle = (static_cast<U>(Cond_AngleFB) + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur[0xB])))) & 0xFFF;
    SetAngleFB(angle);
    const U target = ((2u - cur[8]) & 0xF) << 8;
    if (target != angle) return;
    Field_State[0x137] = 0;
    Cur()[2] = 2;
}

// original 0x42ADB0 (called by Area189_LeaderControl): +9 = 8; +0xC / +0x10 =
// Area189_StepVectors[+8] (the whole byte, unchecked) doubled; +0x14 = ((the
// height at the point +9 steps on) << 16 - +0x3C) / +9, signed (+9 read again
// after the height call; ours aborts on 0, where the original divides by it).
extern "C" void __cdecl Area189_StepBegin(void) {
    Cur()[9] = 8;
    unsigned char* cur = Cur();
    SetLong(cur + 0xC, static_cast<std::int32_t>(static_cast<U>(Long(Mem(at::kArea189StepVectors + cur[8] * 8u))) << 1));
    cur = Cur();
    SetLong(cur + 0x10, static_cast<std::int32_t>(static_cast<U>(Long(Mem(at::kArea189StepVectors + 4 + cur[8] * 8u))) << 1));
    cur = Cur();
    const U n = cur[9];
    const U z = static_cast<U>(Long(cur + 0x10)) * n + static_cast<U>(Long(cur + 0x38));
    const U x = static_cast<U>(Long(cur + 0xC)) * n + static_cast<U>(Long(cur + 0x34));
    const short height = HeightAt(x, z);
    unsigned char* const s = Cur();
    const auto rise = static_cast<std::int32_t>((static_cast<U>(static_cast<std::int32_t>(height)) << 16) - static_cast<U>(Long(s + 0x3C)));
    const unsigned steps = s[9];
    if (steps == 0) bof3::Fatal("Area189_StepBegin: the step count +9 is 0 after the height call (the original divides by it)");
    SetLong(s + 0x14, rise / static_cast<std::int32_t>(steps));
}

// original 0x42AE30 (called by Area189_LeaderControl, Area189_LeaderStep): x
// += +0xC, Field_Kind2X = x; z += +0x10, Field_Kind2Z = z; +0x3C += +0x14;
// MapView_Redraw 2.
extern "C" void __cdecl Area189_StepMove(void) {
    unsigned char* const cur = Cur();
    SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x34)) + static_cast<U>(Long(cur + 0xC))));
    Field_Kind2X = Long(cur + 0x34);
    SetLong(cur + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x38)) + static_cast<U>(Long(cur + 0x10))));
    Field_Kind2Z = Long(cur + 0x38);
    SetLong(cur + 0x3C, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x3C)) + static_cast<U>(Long(cur + 0x14))));
    MapView_Redraw = 2;
}

// original 0x42AE80 (Area189_LeaderStep's tail jump at +9 = 0): the step's
// arrival (below, docs/area_w4e.md section 1).
extern "C" void __cdecl Area189_StepArrive(void) {
    AH_CALL(Area189_ZeroSpeeds)();
    // the frame word: 0x1E0 - the reserve + 8 if any, Field_StatusBits bit 0
    // and Cond_ByteFF cleared, message 5; 0x3C0 - the reserve + 8 if any, bit
    // 0 and Cond_ByteFF set, the word 0, the count + 1, message 6
    const auto frames = static_cast<std::uint16_t>(Word(Mem(at::kWalkFrames)) + 1);
    Field_EdgeBits = static_cast<unsigned short>(Field_EdgeBits + 1);
    SetWord(Mem(at::kWalkFrames), frames);
    if (frames == 0x1E0) {
        const unsigned char reserve = B(at::kWalkReserve);
        if (reserve != 0) B(at::kWalkReserve) = static_cast<unsigned char>(reserve + 8);
        const unsigned char bits = Field_StatusBits;
        Cond_ByteFF = 0;
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        AH_CALL(Msg_OpenScript)(5);
        Field_Request = 2;
    }
    if (Word(Mem(at::kWalkFrames)) == 0x3C0) {
        const unsigned char reserve = B(at::kWalkReserve);
        if (reserve != 0) B(at::kWalkReserve) = static_cast<unsigned char>(reserve + 8);
        const unsigned char bits = Field_StatusBits;
        const unsigned char count = B(at::kWalkCount);
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        Cond_ByteFF = 1;
        SetWord(Mem(at::kWalkFrames), 0);
        B(at::kWalkCount) = static_cast<unsigned char>(count + 1);
        AH_CALL(Msg_OpenScript)(6);
        Field_Request = 2;
    }
    // the reserve counts down (3 a step with Cond_ByteFF 0, else 1, not below
    // 0); at 0 message 1 and the halt. None: an event every (Rand() & 7) + 20
    // * Cond_ByteFF + 7 steps - the fifth in a row Area189_RaiseByte1E,
    // Cond_ByteFE 2 and sound 0x10B; the others Area189_DrainHp, Cond_ByteFE
    // 1 and sound 0x108.
    unsigned char reserve = B(at::kWalkReserve);
    if (reserve != 0) {
        for (unsigned n = Cond_ByteFF == 0 ? 3 : 1; n != 0; --n) {
            if (reserve != 0) --reserve;
        }
        B(at::kWalkReserve) = reserve;
        B(at::kStepsSince) = 0;
        B(at::kEventsSince) = 0;
        if (reserve == 0) {
            AH_CALL(Msg_OpenScript)(1);
            Field_Request = 2;
            AH_CALL(Area189_LeaderHalt)();
            return;
        }
    } else {
        const U r = static_cast<U>(AH_CALL(Rand)()) & 7;
        const auto limit = static_cast<std::int32_t>(r + Cond_ByteFF * 20u + 7u);
        const unsigned char steps = B(at::kStepsSince);
        if (static_cast<std::int32_t>(steps) < limit) {
            B(at::kStepsSince) = static_cast<unsigned char>(steps + 1);
        } else {
            const unsigned char events = B(at::kEventsSince);
            B(at::kStepsSince) = 0;
            if (events >= 4) {
                AH_CALL(Area189_RaiseByte1E)();
                Cond_ByteFE = 2;
                AH_CALL(Sound_PlayEffect)(0x10B);
                B(at::kEventsSince) = 0;
            } else {
                AH_CALL(Area189_DrainHp)();
                Cond_ByteFE = 1;
                AH_CALL(Sound_PlayEffect)(0x108);
                B(at::kEventsSince) = static_cast<unsigned char>(B(at::kEventsSince) + 1);
            }
        }
    }
    if (Field_Request == 2) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    // Cond_Flags row 14 bit 4 clear, the x block 0x1600 or 0x1700 and the z
    // block at most 0x1000: Scena14_LeaveToC4 and the halt.
    const bool bit4 = Test(Row14(), 4);
    unsigned char* cur = Cur();
    if (!bit4) {
        const std::int16_t bx = BlockWord(cur + 0x36);
        if ((bx == 0x1600 || bx == 0x1700) && BlockWord(cur + 0x3A) <= 0x1000) {
            AH_CALL(Scena14_LeaveToC4)();
            AH_CALL(Area189_LeaderHalt)();
            return;
        }
    }
    if (AH_CALL(Scenario_ArriveHook)(Long(cur + 0x34), Long(cur + 0x38)) != 0) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    // each high word moved a block inward when at or past an edge: x at most
    // 0x900 or from 0x1F00, z at most 0xD00 or from 0x1900 (the blocks,
    // signed)
    cur = Cur();
    if (BlockWord(cur + 0x36) <= 0x900) AddWord(cur + 0x36, 0x100);
    if (BlockWord(cur + 0x3A) <= 0xD00) AddWord(cur + 0x3A, 0x100);
    if (BlockWord(cur + 0x36) >= 0x1F00) AddWord(cur + 0x36, -0x100);
    if (BlockWord(cur + 0x3A) >= 0x1900) AddWord(cur + 0x3A, -0x100);
    Field_Kind2X = Long(cur + 0x34);
    const unsigned flags = Field_ScriptFlags;
    Field_Kind2Z = Long(cur + 0x38);
    MapView_Redraw = 2;
    if ((flags & 0x20) == 0 && Field_EdgeBits >= Word(Mem(at::kLeaderPace))) {
        // the pace's steps walked: the edge count 0; a flag for each of six
        // blocks the leader stands on; area 0x7D at (0x190000, 0x190000),
        // flags 4, the facing jittered
        Field_EdgeBits = 0;
        static const struct { std::int16_t x, z; unsigned char flag; } kMarks[] = {
            {0xB00, 0xF00, 0x75}, {0x1500, 0x1100, 0x76}, {0xC00, 0x1400, 0x8B}, {0x1000, 0x1100, 0x8C}, {0x1D00, 0x1700, 0x8D}, {0x1400, 0x1400, 0x8E}};
        for (const auto& m : kMarks) {
            if (BlockWord(cur + 0x36) != m.x || BlockWord(cur + 0x3A) != m.z) continue;
            AH_CALL(Flags_Set)(StoryFlags(), m.flag);
            cur = Cur();
        }
        SetWord(Mem(at::kPlace), 0x7D);
        SetLong(Mem(at::kPendingX), 0x190000);
        SetLong(Mem(at::kPendingZ), 0x190000);
        B(at::kChangeFlags) = 4;
        JitterFacing();
        ChangeToPending();
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if ((flags & 0x100) != 0 || (Field_ScriptFlags2 & 0x40) != 0) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if ((Field_InputHeld & (Word(Mem(at::kButtonMap0)) | 0xB000u)) != 0) {
        AH_CALL(Area189_LeaderControl)();
        return;
    }
    AH_CALL(Area189_LeaderHalt)();
}

// original 0x42B2F0 (called and tail-jumped to by Area189_LeaderControl and
// Area189_StepArrive): Area189_ZeroSpeeds; +0x4B = 0; Field_State +0x137 = 0;
// +2 = 2.
extern "C" void __cdecl Area189_LeaderHalt(void) {
    AH_CALL(Area189_ZeroSpeeds)();
    Cur()[0x4B] = 0;
    Field_State[0x137] = 0;
    Cur()[2] = 2;
}

// original 0x42B320 (called by Area189_LeaderControl): Input_Pressed bit 11:
// the place Area189_ExitPlaces[Cond_ByteFF 0 ? 1 : 0] (area, x and z bytes
// << 16) pending with flags 4; the leader's x, z and Game_AreaNumber saved to
// 0x904148 / 0x90414C / 0x904150, the byte 0x904152 + 1; the facing
// jittered; Field_ChangeArea from the pending cells; al 1. Else al 0.
extern "C" unsigned char __cdecl Area189_ExitButton(void) {
    if ((Input_Pressed & 0x800) == 0) return 0;
    const unsigned e = Cond_ByteFF == 0 ? 1u : 0u;
    B(at::kChangeFlags) = 4;
    const U place = at::kArea189ExitPlaces + e * 3;
    SetWord(Mem(at::kPlace), B(place));
    SetLong(Mem(at::kPendingX), static_cast<std::int32_t>(static_cast<U>(B(place + 1)) << 16));
    SetLong(Mem(at::kPendingZ), static_cast<std::int32_t>(static_cast<U>(B(place + 2)) << 16));
    const unsigned char* const cur = Cur();
    SetLong(Mem(at::kSavedX), Long(cur + 0x34));
    SetLong(Mem(at::kSavedZ), Long(cur + 0x38));
    const unsigned char saved = B(at::kSavedCount);
    SetWord(Mem(at::kSavedArea), Game_AreaNumber);
    B(at::kSavedCount) = static_cast<unsigned char>(saved + 1);
    JitterFacing();
    ChangeToPending();
    return 1;
}

// original 0x42B400 (called by Area189_LeaderControl): Field_MenuButton
// pressed (Input_Pressed) outside Field_ScriptFlags bit 6:
// Sound_PlayEffect(0x105), Field_Request 1, +2 = 2 (Sprite_Current read after
// the call), al 1. Else al 0.
extern "C" unsigned char __cdecl Area189_MenuButton(void) {
    if ((Field_MenuButton & Input_Pressed) == 0) return 0;
    if ((Field_ScriptFlags & 0x40) != 0) return 0;
    AH_CALL(Sound_PlayEffect)(0x105);
    unsigned char* const cur = Cur();
    Field_Request = 1;
    cur[2] = 2;
    return 1;
}

// original 0x42B440 (called by Area189_LeaderStart, _StepArrive, _LeaderHalt):
// +0xC, +0x10, +0x14 = 0.
extern "C" void __cdecl Area189_ZeroSpeeds(void) {
    SetLong(Cur() + 0xC, 0);
    SetLong(Cur() + 0x10, 0);
    SetLong(Cur() + 0x14, 0);
}

// original 0x42B460 (called by Area189_LeaderControl): Input_Held bit 12:
// +0x4B = 1, al 2. Bit 13: +0x4B = 2, +8 + 1, +0xB = 0xF0; else bit 15: +0x4B =
// 3, +8 - 1, +0xB = 0x10 - either: +8 &= 0xF, al 1. None: al 0.
extern "C" unsigned char __cdecl Area189_TurnInput(void) {
    const unsigned held = Input_Held;
    if ((held & 0x1000) != 0) {
        Cur()[0x4B] = 1;
        return 2;
    }
    if ((held & 0x2000) != 0) {
        Cur()[0x4B] = 2;
        Cur()[8] = static_cast<unsigned char>(Cur()[8] + 1);
        Cur()[0xB] = 0xF0;
    } else if ((held & 0x8000) != 0) {
        Cur()[0x4B] = 3;
        Cur()[8] = static_cast<unsigned char>(Cur()[8] - 1);
        Cur()[0xB] = 0x10;
    } else {
        return 0;
    }
    Cur()[8] = static_cast<unsigned char>(Cur()[8] & 0xF);
    return 1;
}

// original 0x42B4E0 (called by Area189_StepArrive): for each of the seven
// actor records walked, the byte +0x1E below 9: + 1 and Char_RecalcStats.
extern "C" void __cdecl Area189_RaiseByte1E(void) {
    for (unsigned r = 0; r < at::kCharWalked; ++r) {
        unsigned char& b = B(at::kCharByte1E + r * at::kCharStride);
        if (b >= 9) continue;
        b = static_cast<unsigned char>(b + 1);
        AH_CALL(Char_RecalcStats)(Mem(at::kCharRecords + r * at::kCharStride));
    }
}

// original 0x42B510 (called by Area189_StepArrive): for each of the seven
// actor records walked, d = (+0x40 * 2 + 0x32) / 100; the word +0x18 above d:
// - d; else 1 (0 included).
extern "C" void __cdecl Area189_DrainHp(void) {
    for (unsigned r = 0; r < at::kCharWalked; ++r) {
        unsigned char* const w = Mem(at::kCharWord18 + r * at::kCharStride);
        const U bound = Word(w + 0x28);
        U d = (bound * 2u + 0x32u) / 100u;
        const U hp = Word(w);
        if (hp <= (d & 0xFFFF)) d = hp - 1;
        SetWord(w, hp - d);
    }
}

// ===========================================================================
// Area 191 (descriptor 0x647BC8; PSX 0x801F418C)
// ===========================================================================

// original 0x42B550 (areas 191 and 192 +0x34[0]): message 0x68 for answer 0,
// else 0xFFFF.
extern "C" void __cdecl Area191_ChoiceMessage68(void) { SetMessage(Answer() != 0 ? 0xFFFF : 0x68); }

// original 0x42B570 (area 191 +0x34[2]): FocusPair over Area191_FocusPairs.
extern "C" void __cdecl Area191_ChoiceFocusPair(void) { FocusPair(at::kArea191FocusPairs); }

// original 0x42B5B0 (areas 191 and 192 +0x34[3]): message 0x6C for answer 0,
// else 0xFFFF.
extern "C" void __cdecl Area191_ChoiceMessage6C(void) { SetMessage(Answer() != 0 ? 0xFFFF : 0x6C); }

// original 0x42B5D0 (area 191 +0x34[5]): message 0xFFFF; answer 1:
// ScriptFlags_Set40, tail kind 53 (Area191_Tail53) at state 0x1E; 2:
// ScriptFlags_Set40, the run: MoveScript_Var7 1, step 0x14.
extern "C" void __cdecl Area191_ChoiceTail53(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 1) {
        ArmTail(0x35, 0x1E);
        return;
    }
    if (a != 2) return;
    AH_CALL(ScriptFlags_Set40)();
    SetRun(1, 0x14);
}

// original 0x42B610 (areas 191 and 192 +0x3C[0] = +0x34[6]; PSX 0x801F30DC):
// ScriptFlags_Set40; the active member's +0x80 bit 0 cleared and its word
// +0x8A + 1 (Field_ActiveMember read after the call and again).
extern "C" void __cdecl Area191_MemberNext(void) {
    AH_CALL(ScriptFlags_Set40)();
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    AddWord(Field_ActiveMember + 0x8A, 1);
}

// original 0x42B640 (area 191 +0x3C[1] = +0x34[7]; PSX 0x801F3130):
// Area191_MemberNext's body, then the run: MoveScript_Var7 2, step 0x19.
extern "C" void __cdecl Area191_MemberNextRun(void) {
    AH_CALL(ScriptFlags_Set40)();
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    AddWord(Field_ActiveMember + 0x8A, 1);
    SetRun(2, 0x19);
}

// original 0x42B680 (area 191 +0x3C[2] = +0x34[8]; PSX 0x801F31A0):
// Area191_ScaleStates by Sprite_Current +4 (unchecked there; ours aborts past
// its three).
extern "C" void __cdecl Area191_RunScale(void) {
    StateEntry("Area191_RunScale", at::kArea191ScaleStates, at::kArea191ScaleStateCount, Cur()[4])();
}

// original 0x42B6A0 (Area191_ScaleStates 0): +0x48 = 2; +0x40, +0x44 =
// 0x10000; +0xA = 0xFF; the dword +0x70 = 1; +0x5D..+0x5F = 0x50, 0x3C, 0; +4
// = 1; the script back 2.
extern "C" void __cdecl Area191_ScaleStart(void) {
    unsigned char* const cur = Cur();
    cur[0x48] = 2;
    SetLong(cur + 0x40, 0x10000);
    SetLong(cur + 0x44, 0x10000);
    cur[0xA] = 0xFF;
    SetLong(cur + 0x70, 1);
    cur[0x5D] = 0x50;
    cur[0x5E] = 0x3C;
    cur[0x5F] = 0;
    cur[4] = 1;
    ScriptBack2();
}

// Area 191's scale states 1 and 2: x += +0xC; +0x40 and +0x44 += step; +0xA -
// 1; its low two bits 0: +4 = `flip`; +0xA at 0: +4 = 0 (done); else the
// script back 2.
static void ScaleTick(std::int32_t step, unsigned char flip) {
    unsigned char* const cur = Cur();
    SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x34)) + static_cast<U>(Long(cur + 0xC))));
    SetLong(cur + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x40)) + static_cast<U>(step)));
    SetLong(cur + 0x44, static_cast<std::int32_t>(static_cast<U>(Long(cur + 0x44)) + static_cast<U>(step)));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    if ((cur[0xA] & 3) == 0) cur[4] = flip;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    ScriptBack2();
}

// original 0x42B710 (Area191_ScaleStates 1): ScaleTick(+0x2000, to state 2).
extern "C" void __cdecl Area191_ScaleGrow(void) { ScaleTick(0x2000, 2); }

// original 0x42B780 (Area191_ScaleStates 2): ScaleTick(-0x2000, to state 1).
extern "C" void __cdecl Area191_ScaleShrink(void) { ScaleTick(-0x2000, 1); }

// original 0x42B7F0 (Field_ModeTailKinds[53]): by the s8 state (a byte table
// of 31 at +0x1D8 into a jump table of 8 at +0x1B8, both inside the extent;
// the ja bounds it unsigned): 0 - message 0x6D, request 2, state 1 (which
// does nothing: the message's choice moves it on). 2 - once Field_Request is
// not 2: Transition_Start(0), state 3. 3 - once MoveScript_WaitWordDA is 0:
// Draw_PassFlags 0, Sound_StopMusic, 0x42C2D0, Sound_LoadStream(0), state 4.
// 4 - once Sound_StreamDone answers (all of eax): story flag 0x82, the walk's
// word 0x90405C, 0x929EC1, 0x9036D0 0, its count + 1, Field_StatusBits bit 0,
// state 0xA. 0xA - once not 2: ScriptFlags_Clear40, Field_ChangeArea(the
// saved area, x, z, 4), Field_ScriptFlags2 bit 6, the byte 0x904152 0, story
// flag 0x77 cleared, kind and state 0. 0x14 - once not 2: ScriptFlags_Clear40,
// kind and state 0. 0x1E - once not 2: ScriptFlags_Clear40,
// Field_ChangeArea(0x96, 0x180000, 0x300000, 1), the same three, then
// Field_StatusBits bit 0 cleared, kind and state 0. Every other state:
// nothing.
extern "C" void __cdecl Area191_Tail53(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(Msg_OpenScript)(0x6D);
        Field_Request = 2;
        B(at::kTailState) = 1;
        return;
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(0);
        B(at::kTailState) = 3;
        return;
    case 3:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        AH_CALL(Sound_StopMusic)();
        AH_AT(void (__cdecl*)(), at::kRestoreRecords)();
        AH_CALL(Sound_LoadStream)(0);
        B(at::kTailState) = 4;
        return;
    case 4: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x82);
        const unsigned char count = B(at::kWalkCount);
        const unsigned char bits = Field_StatusBits;
        SetWord(Mem(at::kWalkFrames), 0);
        B(at::kStepsSince) = 0;
        B(at::kEventsSince) = 0;
        B(at::kWalkCount) = static_cast<unsigned char>(count + 1);
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        B(at::kTailState) = 0xA;
        return;
    }
    case 0xA:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(Word(Mem(at::kSavedArea)), Long(Mem(at::kSavedX)), Long(Mem(at::kSavedZ)), 4);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x40);
        B(at::kSavedCount) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0x14:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0x1E: {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x96, 0x180000, 0x300000, 1);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x40);
        B(at::kSavedCount) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        const unsigned char bits = Field_StatusBits;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        return;
    }
    default: return;
    }
}

// original 0x42B9F0 (Area_StepHook's case for area 0xBF): the cells the step
// covers - (x, z) by their high words, and the next cell in x when x's low
// word is not 0, in z when z's is not, and the diagonal when both - counted
// where AreaMap_ByteAt answers 0xA6; any: ScriptFlags_Set40, tail kind 53 at
// state 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area191_StepHook(long x, long z) {
    const U xv = static_cast<U>(x), zv = static_cast<U>(z);
    const auto cx = static_cast<short>(xv >> 16), cz = static_cast<short>(zv >> 16);
    unsigned char n = 0;
    if (AH_CALL(AreaMap_ByteAt)(cx, cz) == 0xA6) n = 1;
    if ((xv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), cz) == 0xA6) ++n;
    if ((zv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(cx, static_cast<short>(cz + 1)) == 0xA6) ++n;
    if ((xv & 0xFFFF) != 0 && (zv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), static_cast<short>(cz + 1)) == 0xA6) ++n;
    if (n == 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x35;
    B(at::kTailState) = 0;
    return 1;
}

// original 0x42BA90 (called raw by chapter code: 0x5677F3, 0x567833,
// 0x567873, 0x5678B3, 0x5678F3 (SC13) and 0x56AEE1 (SC15)): the message a
// member's talk opens. i = the index of the member byte (`who`'s low byte) in
// Area191_TalkKeys (5 when none); a = i * 9; b = the walk count, at most 8; c
// = the byte +0x1E of the actor record MoveScript_EffectState[who] names (both
// unchecked). Cond_Flags row 14 bit 0xA set: Area191_TalkMessageB(a, who, b,
// c). Else a + 8 for c from 8, + 7 for c above 4 with b below 7, else +
// Area191_TalkSteps[b]; the word from Area191_TalkMessagesB (the chapter
// row's flag 2) or _A by a.
extern "C" unsigned short __cdecl Area191_TalkMessage(unsigned who) {
    const auto key = static_cast<unsigned char>(who);
    unsigned char i = 0;
    while (B(at::kArea191TalkKeys + i) != key) {
        ++i;
        if (i >= 5) break;
    }
    auto a = static_cast<unsigned char>(i * 9);
    unsigned char b = B(at::kWalkCount);
    if (b > 8) b = 8;
    const unsigned char c = B(at::kCharByte1E + MoveScript_EffectState[key] * at::kCharStride);
    if (Test(Row14(), 0xA)) return AH_CALL(Area191_TalkMessageB)(a, who, b, c);
    if (c >= 8) {
        a = static_cast<unsigned char>(a + 8);
    } else if (c > 4 && b < 7) {
        a = static_cast<unsigned char>(a + 7);
    } else {
        a = static_cast<unsigned char>(a + B(at::kArea191TalkSteps + b));
    }
    if (Test(FlagRow(), 2)) return B(at::kArea191TalkMessagesB + a);
    return B(at::kArea191TalkMessagesA + a);
}

// original 0x42BBB0 (a, who, b, c; called by Area191_TalkMessage): a +
// Area191_TalkStepsC[b] for c below 5, else a + 8; the word from
// Area191_TalkMessagesC by it. `who` is not read.
extern "C" unsigned short __cdecl Area191_TalkMessageB(unsigned a, unsigned who, unsigned b, unsigned c) {
    (void)who;
    const auto r = static_cast<unsigned char>((c & 0xFF) < 5 ? (a & 0xFF) + B(at::kArea191TalkStepsC + (b & 0xFF)) : (a & 0xFF) + 8);
    return B(at::kArea191TalkMessagesC + r);
}

// original 0x42BBF0 (area 191 +0x40; PSX 0x801F3964), with 0x42BC20, the body
// it tail-jumps to (reached by nothing else): nothing with Cond_ByteFA 0xE and
// the chapter row's flag 5, nor with story flag 0x77. Else a list k by actor
// record 0's byte +0x1E (read first): row 14 bit 0xA clear - 2 from 9, 1 from
// 5, else the walk count at least 5; set - 2 from 5, else the count at least
// 5. Six times: Sprite_FindFree to the word DamageScratch; not 0xFF:
// EventOp_0x(Area191_ObjectLists[k] + 0x11 * n).
extern "C" void __cdecl Area191_Init(void) {
    if (Cond_ByteFA == 0xE && Test(FlagRow(), 5)) return;
    const unsigned char level = B(at::kCharByte1E);
    if (Test(StoryFlags(), 0x77)) return;
    unsigned char k;
    if (!Test(Row14(), 0xA)) {
        k = level >= 9 ? 2 : level >= 5 ? 1 : static_cast<unsigned char>(B(at::kWalkCount) >= 5);
    } else {
        k = level >= 5 ? 2 : static_cast<unsigned char>(B(at::kWalkCount) >= 5);
    }
    for (unsigned n = 0; n < 6; ++n) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(at::kDamageScratch), slot);
        if (slot == 0xFF) continue;
        const U list = static_cast<U>(Long(Mem(at::kArea191ObjectLists + k * 4u)));
        AH_CALL(EventOp_0x)(Mem(list + n * at::kEventOpRecord));
    }
}

// original 0x42BCE0 (EffectKind18_States[77], 0x6541A0): story flag 0x77 set:
// the walk's word 0x90405C, 0x929EC1, 0x9036D0 0, its count + 1,
// Field_StatusBits bit 0; a tail jump to Effect_Release.
extern "C" void __cdecl Area191_Kind18Flag77(void) {
    if (!Test(StoryFlags(), 0x77)) return;
    const unsigned char count = B(at::kWalkCount);
    SetWord(Mem(at::kWalkFrames), 0);
    B(at::kStepsSince) = 0;
    B(at::kEventsSince) = 0;
    const unsigned char bits = Field_StatusBits;
    B(at::kWalkCount) = static_cast<unsigned char>(count + 1);
    Field_StatusBits = static_cast<unsigned char>(bits | 1);
    AH_CALL(Effect_Release)();
}

// original 0x42BD30 (areas 191 and 192 +0x34[1]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, the run: MoveScript_Var7 1, step 0x14.
extern "C" void __cdecl Area191_ChoiceRun1(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    SetRun(1, 0x14);
}

void AreaW4e_Inject() {
    if (bof3::WantsShadow("area_w4e")) area_w4e::SelfTest();
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
    BOF3_INJECT(Area189_Init);
    BOF3_INJECT(Area189_ChoiceTail50A);
    BOF3_INJECT(Area189_ChoiceTail50B);
    BOF3_INJECT(Area189_Tail50);
    BOF3_INJECT(Area189_LeaderRun);
    BOF3_INJECT(Area189_LeaderStart);
    BOF3_INJECT(Area189_LeaderProject);
    BOF3_INJECT(Area189_LeaderControl);
    BOF3_INJECT(Area189_LeaderStep);
    BOF3_INJECT(Area189_LeaderTurn);
    BOF3_INJECT(Area189_StepBegin);
    BOF3_INJECT(Area189_StepMove);
    BOF3_INJECT(Area189_StepArrive);
    BOF3_INJECT(Area189_LeaderHalt);
    BOF3_INJECT(Area189_ExitButton);
    BOF3_INJECT(Area189_MenuButton);
    BOF3_INJECT(Area189_ZeroSpeeds);
    BOF3_INJECT(Area189_TurnInput);
    BOF3_INJECT(Area189_RaiseByte1E);
    BOF3_INJECT(Area189_DrainHp);
    BOF3_INJECT(Area191_ChoiceMessage68);
    BOF3_INJECT(Area191_ChoiceFocusPair);
    BOF3_INJECT(Area191_ChoiceMessage6C);
    BOF3_INJECT(Area191_ChoiceTail53);
    BOF3_INJECT(Area191_MemberNext);
    BOF3_INJECT(Area191_MemberNextRun);
    BOF3_INJECT(Area191_RunScale);
    BOF3_INJECT(Area191_ScaleStart);
    BOF3_INJECT(Area191_ScaleGrow);
    BOF3_INJECT(Area191_ScaleShrink);
    BOF3_INJECT(Area191_Tail53);
    BOF3_INJECT(Area191_StepHook);
    BOF3_INJECT(Area191_TalkMessage);
    BOF3_INJECT(Area191_TalkMessageB);
    BOF3_INJECT(Area191_Init);
    BOF3_INJECT(Area191_Kind18Flag77);
    BOF3_INJECT(Area191_ChoiceRun1);
}

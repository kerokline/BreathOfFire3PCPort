// World 2's areas 108 and 110..113: the PSX's BIN/WORLD02/AREA108 and
// AREA110..113.EMI compiled into the exe at 0x4168E0..0x418BE0
// (Area_Descriptors entries 108..113; 109 has no code). Round ten, group AR2F:
// the band's 53 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index that stay inside the image's .data are kept (area
// 108's place tables and MoveScript_EffectState by a byte, area 111's grid by
// the leader's cell, the extra objects by a byte); the dispatchers through
// the areas' state tables abort past the table (what follows is data, which
// the original would jump into), area 111's handler 8 aborts where its dword
// index would read outside the image, and its handler 5 aborts where its
// final write would land outside the 28-byte grid (docs/area_w2f.md section
// 6). Every call goes through the harness (AH_CALL / AH_AT), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; the
// callees of the group's own are called the same way, so each function is
// fuzzed alone.
#include "game/area_w2f.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2f::at;
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
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// Sprite_ObjectsExtra record k (0xA4 bytes; k not checked, as the originals).
unsigned char* Extra(unsigned k) { return Mem(at::kSpriteObjectsExtra + k * at::kExtraStride); }
// Party record m (ObjTrio, 0x14C bytes; m not checked).
unsigned char* Record(unsigned m) { return Mem(at::kLeader + m * at::kPartyStride); }
// Field object k (Sprite_Objects, 0xA4 bytes).
unsigned char* ObjectAt(unsigned k) { return Sprite_Objects + k * 0xA4; }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }
void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }
void Clear(unsigned index) { AH_CALL(Flags_Clear)(StoryFlags(), index); }
// The tail's word timer counted down (a 16-bit dec): the new value.
unsigned DecTimer() {
    const unsigned v = (Word(Mem(at::kTailTimer)) - 1u) & 0xFFFFu;
    SetWord(Mem(at::kTailTimer), v);
    return v;
}
// The movement script's position (MoveScript_Object's word +0xA) moved by v.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The map setters as the originals pass them: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// `(v - edx) >> 1` after cdq: the division by 2 that truncates toward 0.
int Half(int v) { return v / 2; }
// The x87-free `sar reg, 1`: the division by 2 that rounds down.
int Floor2(int v) { return v >> 1; }
// A signed 16-bit word read as an int.
int S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// Field_State's party slot + 1 as the originals compute it: (Field_State -
// ObjTrio) / 0x14C (a signed divide), + 1, a byte.
unsigned char FieldStateSlot() {
    const auto d = static_cast<std::int32_t>(Addr(Field_State) - at::kLeader);
    return static_cast<unsigned char>(d / 0x14C + 1);
}
// Field_ActiveMember's index among Sprite_ObjectsExtra's records as the
// originals compute it: (Field_ActiveMember - 0x802000) / 0xA4, signed.
std::int32_t ActiveMemberExtra() {
    return static_cast<std::int32_t>(Addr(Field_ActiveMember) - at::kSpriteObjectsExtra) / static_cast<std::int32_t>(at::kExtraStride);
}

// ---- area 111's grid ----

// The nibble of the 7 x 7 grid at (col, row): the byte (col >> 1) + row * 4,
// its low nibble for an odd column, its high one for an even column. The
// originals index by bytes they do not check; every such read stays inside
// .data (at most 0x675C00 + 0x7F + 0x3FC).
unsigned char Nibble(unsigned col, unsigned row) {
    const unsigned char b = B(at::kArea111Grid + (col >> 1) + row * 4);
    return (col & 1) != 0 ? static_cast<unsigned char>(b & 0xF) : static_cast<unsigned char>(b >> 4);
}
// The leader's grid cell, as three functions compute it: ((s16 x high word -
// 0xB) / 2, (s16 z high word - 0xB1) / 2), each truncated toward 0, as bytes.
struct Cell {
    unsigned char col, row;
};
Cell LeaderCell() {
    return {static_cast<unsigned char>(Half(S16(Mem(at::kLeaderXHigh)) - 0xB)), static_cast<unsigned char>(Half(S16(Mem(at::kLeaderZHigh)) - 0xB1))};
}

// ---- area 108 ----

// A mask of the party records 1..count-1 whose high words pass: bit i for
// record i + 1, as the originals' byte `shl` (the count masked to 5 bits, so
// no bit past the eighth). A count of 1 or less: 0.
template <typename Pass> unsigned char MemberMask(Pass pass) {
    const unsigned count = Field_MemberCount;
    unsigned char mask = 0;
    if (count <= 1) return 0;
    for (unsigned i = 0; i + 1 < count; ++i) {
        const unsigned char* const r = Record(i + 1);
        if (!pass(static_cast<std::int16_t>(Word(r + 0x36)), static_cast<std::int16_t>(Word(r + 0x3A)))) continue;
        const unsigned s = i & 31;
        if (s < 8) mask = static_cast<unsigned char>(mask | (1u << s));
    }
    return mask;
}
bool Within2(std::int16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 2; }

// The active member's script: +0x83 = script, word +0x8A = 0.
void MemberScript(unsigned char script) {
    Field_ActiveMember[0x83] = script;
    SetWord(Field_ActiveMember + 0x8A, 0);
}

// Handlers 5 and 6: the running object put at the table's entry (extra
// object 1's +0x83 - 1, a byte; unchecked, it reads inside .data): word +0x36
// = the x byte, word +0x34 = 0x8000 when the x flag is set, word +0x3A = the
// z byte, word +0x38 likewise.
void PlaceAtCell(U table) {
    const unsigned index = static_cast<unsigned char>(Extra(1)[0x83] - 1);
    const unsigned char* const e = Mem(table + index * 4);
    SetWord(Sprite_Current + 0x36, e[0]);
    SetWord(Sprite_Current + 0x34, e[1] != 0 ? 0x8000u : 0u);
    SetWord(Sprite_Current + 0x3A, e[2]);
    SetWord(Sprite_Current + 0x38, e[3] != 0 ? 0x8000u : 0u);
}

// ---- area 110 ----

// Area 110's init (0x417580, area 72's code over area 110's tables): a
// weighted draw - Rand() & 0x3F walked down the eight weights, the first it
// falls below chosen (none: 8); field objects 0..7 but the chosen one get +0
// = 0; the chosen one put at one of the eight cells (Rand() & 7: x and z
// bytes << 16), its word +0x3E the ground there; Field_EdgeBits = the
// leader's zone counter less 5 (either way).
void PlaceRandomObject(U cells, U weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        const auto x = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2 + 1)) << 16);
        SetLong(object + 0x34, x);
        SetLong(object + 0x38, z);
        const long ground = AH_CALL(AreaMap_Elevation)(Long(object + 0x34), z);
        SetWord(object + 0x3E, static_cast<unsigned>(ground));
    }
    Field_EdgeBits = static_cast<unsigned short>(Long(Mem(at::kLeaderZone)) - 5);
}

}  // namespace

// ===========================================================================
// Area 108 (descriptor 0x61E598): nineteen choices, sixteen handlers (the
// handlers are choices 3..18 but 2), a cell hook, tail kind 12, and a helper.
// Choice 2 is area 22's Area22_ArmTailOnYes, choice 15 = handler 12 area 67's
// Area67_SetBit24, choices 16, 17 = handlers 13, 14 area 57's
// Area57_StopMusic / Area57_ResumeSound; +0x40 is the shared ret 0x437CC0.
// ===========================================================================

// original 0x4168E0 (area 108 +0x34[0]): the message word 0xFFFF; by the s8
// answer: 0 - sub-kind 3, story flag 0x1E; 1 - sub-kind 2, flag 0x1F; then
// tail kind 5. Any other answer: nothing more.
extern "C" void __cdecl Area108_ChoiceArmTail5(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(0xFFFF);
    unsigned flag;
    if (answer == 0) {
        B(at::kTailSub) = 3;
        flag = 0x1E;
    } else if (answer == 1) {
        B(at::kTailSub) = 2;
        flag = 0x1F;
    } else {
        return;
    }
    Set(flag);
    B(at::kTailKind) = 5;
}

// original 0x416930 (area 108 +0x34[1]): the message word 0xFFFF; answer 0:
// tail kind 0xA, state 0, sub-kind 4.
extern "C" void __cdecl Area108_ChoiceArmTailA(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    B(at::kTailKind) = 0xA;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 4;
}

// original 0x416960 (area 108 +0x3C[0] = +0x34[3]; PSX 0x801F43A4): counter 1
// bits 0 and 1 cleared.
extern "C" void __cdecl Area108_ClearCounter1Bits0(void) { B(at::kCounter1) = static_cast<unsigned char>(B(at::kCounter1) & 0xFC); }
// original 0x416970 (area 108 +0x3C[1]; PSX 0x801F43C0): bits 2 and 3.
extern "C" void __cdecl Area108_ClearCounter1Bits2(void) { B(at::kCounter1) = static_cast<unsigned char>(B(at::kCounter1) & 0xF3); }
// original 0x416980 (area 108 +0x3C[2]; PSX 0x801F43DC): bits 4 and 5.
extern "C" void __cdecl Area108_ClearCounter1Bits4(void) { B(at::kCounter1) = static_cast<unsigned char>(B(at::kCounter1) & 0xCF); }
// original 0x416990 (area 108 +0x3C[3]; PSX 0x801F43F8): bits 6 and 7.
extern "C" void __cdecl Area108_ClearCounter1Bits6(void) { B(at::kCounter1) = static_cast<unsigned char>(B(at::kCounter1) & 0x3F); }

// original 0x4169A0 (area 108 +0x3C[4]; PSX 0x801F4414): the active member's
// +0x80 bit 0 cleared; unless the leader's member id (+0x89) is 2, nothing
// more. The first of Area108_Places whose direction is the leader's (+8 & 7)
// and whose x, z are extra object 1's high words (+0x36, +0x3A) picks the
// scene (none: nothing more): Party_DropIn(a mask of the other members in
// the place's strip), the member's script (+0x83, +0x8A = 0) - place 0: 1,
// 2, 3 or 4 by story flags 0x18..0x1A (2 with Area108_SetCells(1)); 1: 5; 2:
// 6 with Area108_SetCells(0); 3: 7; 4: 8, or 9 with flag 0x1B. Then
// MoveScript_PartyRecords record 1's bit 0, ScriptFlags_Set40, tail kind 0xC,
// state 0, timer 0xF.
extern "C" void __cdecl Area108_PlaceScene(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(at::kLeaderMember) != 2) return;
    const unsigned direction = B(at::kLeaderPose) & 7u;
    const std::uint16_t x = Word(Extra(1) + 0x36);
    const std::uint16_t z = Word(Extra(1) + 0x3A);
    unsigned place = 0;
    for (; place < at::kArea108PlaceCount; ++place) {
        const unsigned char* const e = Mem(at::kArea108Places + place * 3);
        if (direction == e[2] && x == e[0] && z == e[1]) break;
    }
    switch (place) {
    case 0:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx > 0x27 && Within2(mz, 0x53); }));
        if (!Test(0x18)) {
            MemberScript(1);
        } else if (!Test(0x19)) {
            AH_CALL(Area108_SetCells)(1);
            MemberScript(2);
        } else if (!Test(0x1A)) {
            MemberScript(3);
        } else {
            MemberScript(4);
        }
        break;
    case 1:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx < 0x39 && Within2(mz, 0x53); }));
        MemberScript(5);
        break;
    case 2:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mz > 0x3A && Within2(mx, 0x33); }));
        AH_CALL(Area108_SetCells)(0);
        MemberScript(6);
        break;
    case 3:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx > 0x21 && Within2(mz, 0x41); }));
        MemberScript(7);
        break;
    case 4:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mz < 0x41 && Within2(mx, 0x1C); }));
        MemberScript(Test(0x1B) ? 9 : 8);
        break;
    default: return;
    }
    B(at::kByte803490) = static_cast<unsigned char>(B(at::kByte803490) | 1);
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0xC;
    B(at::kTailState) = 0;
    SetWord(Mem(at::kTailTimer), 0xF);
}

// original 0x416D60 (area 108 +0x3C[5]; PSX 0x801F4918): PlaceAtCell over
// Area108_PlaceCellsA.
extern "C" void __cdecl Area108_PlaceAtCellA(void) { PlaceAtCell(at::kArea108PlaceCellsA); }
// original 0x416DE0 (area 108 +0x3C[6]; PSX 0x801F4998): over
// Area108_PlaceCellsB.
extern "C" void __cdecl Area108_PlaceAtCellB(void) { PlaceAtCell(at::kArea108PlaceCellsB); }

// original 0x416E60 (area 108 +0x3C[7]; PSX 0x801F4A18): story flag 0x1E:
// the active member's script 0x12; else flag 0x1F: 0x11; either way its
// script position +0x8A = 0xFFFE.
extern "C" void __cdecl Area108_ScriptByFlags1E(void) {
    if (Test(0x1E)) {
        Field_ActiveMember[0x83] = 0x12;
    } else if (Test(0x1F)) {
        Field_ActiveMember[0x83] = 0x11;
    }
    SetWord(Field_ActiveMember + 0x8A, 0xFFFE);
}

// original 0x416EC0 (area 108 +0x3C[8]; PSX 0x801F4A98): the active member's
// +0x80 bit 0 cleared; MoveScript_EffectState[the leader's +0x89] (a byte
// index, unchecked; it reads inside .data) at 5: story flag 0x30,
// MoveCmd_TestFB(1, 0xB), the running object's +0 = 0.
extern "C" void __cdecl Area108_FlagIfEffectState5(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(0x66972C + B(at::kLeaderMember)) != 5) return;   // MoveScript_EffectState
    Set(0x30);
    AH_CALL(MoveCmd_TestFB)(1, 0xB);
    Sprite_Current[0] = 0;
}

// original 0x416F10 (area 108 +0x3C[9]; PSX 0x801F4B14): Area108_FadeStates
// by Sprite_Current[4].
extern "C" void __cdecl Area108_FadeRun(void) {
    StateEntry("Area108_FadeRun", at::kArea108FadeStates, at::kArea108FadeStateCount, Sprite_Current[4])();
}

// original 0x416F30 (Area108_FadeStates[0]): Sprite_SetTint(the running
// object, 0, 0, 0, 1) to the active member's +0x9F; the object's +0 bit 0x20
// set and 0x40 cleared, +0x5C = 1, +0x5F, +0x5E, +0x5D = 0x80, state +4 = 1;
// the member's script position - 2.
extern "C" void __cdecl Area108_FadeBegin(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = tint;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x416FB0 (Area108_FadeStates[1]): each of +0x5D, +0x5E, +0x5F
// below 0xD0 (signed bytes) + 1; all three at 0xD0: Tint_Release(the
// member's +0x9F), +0 bit 0x20 cleared, +0x5C, +0x5F, +0x5E, +0x5D and the
// state 0; else the member's script position - 2.
extern "C" void __cdecl Area108_FadeStep(void) {
    unsigned char* const cur = Sprite_Current;
    for (unsigned off = 0x5D; off <= 0x5F; ++off)
        if (static_cast<signed char>(cur[off]) < static_cast<signed char>(0xD0)) cur[off] = static_cast<unsigned char>(cur[off] + 1);
    if (cur[0x5D] == 0xD0 && cur[0x5E] == 0xD0 && cur[0x5F] == 0xD0) {
        AH_CALL(Tint_Release)(Field_ActiveMember[0x9F]);
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
        Sprite_Current[0x5C] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5D] = 0;
        Sprite_Current[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x417060 (area 108 +0x3C[10]; PSX 0x801F4D60): Effect_FindFree to
// the running object's +0xB; none: the script position - 2.
extern "C" void __cdecl Area108_FindEffectSlot(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    if (Sprite_Current[0xB] == 0xFF) ScriptStep(-2);
}

// original 0x417090 (area 108 +0x3C[11]; PSX 0x801F4DC8): Effect_FindFree to
// +0xB; none: the script position - 2. A slot: z +0x38 = 0x418000, x +0x34 =
// 0x158000, +0 = 1, kind +5 = 0x51, +1 = 0xA, +0x3C = the ground there << 16.
extern "C" void __cdecl Area108_SpawnEffect51(void) {
    const unsigned char found = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = found;
    const unsigned char slot = Sprite_Current[0xB];
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    unsigned char* const e = EffectAt(slot);
    SetLong(e + 0x38, 0x418000);
    SetLong(e + 0x34, 0x158000);
    e[0] = 1;
    e[5] = 0x51;
    e[1] = 0xA;
    const long ground = AH_CALL(AreaMap_Elevation)(Long(e + 0x34), Long(e + 0x38));
    SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16));
}

// original 0x417100 (area 108 +0x3C[15] = +0x34[18]; PSX 0x801F4F0C):
// Field_Request 5: Area108_SetCells(0); else the script position - 2.
extern "C" void __cdecl Area108_CellsIfRequest5(void) {
    if (Field_Request == 5) {
        AH_CALL(Area108_SetCells)(0);
        return;
    }
    ScriptStep(-2);
}

// original 0x417120 (area 108's cell hook, Area_CellHooks): the first of
// Area108_HookCells whose x and z bytes are the cell's and whose direction
// (& 0xF) is the leader's whole pose byte; none, or story flag 0x1C set: al
// 0. Else the entry's flag toggled, Sound_PlayEffect(0x206), and a free
// effect slot (if any): flag 0x1C set, the record +0 = 1, kind +5 = 0x18,
// +0xB = 0x1B, +1 = 0, words +0x36 / +0x3A the cell's x and z; al 1.
extern "C" unsigned char __cdecl Area108_CellHook(long x, long z) {
    const auto cx = static_cast<unsigned char>(x);
    const auto cz = static_cast<unsigned char>(z);
    const unsigned char pose = B(at::kLeaderPose);
    unsigned i = 0;
    for (; i < at::kArea108HookCellCount; ++i) {
        const unsigned char* const e = Mem(at::kArea108HookCells + i * 4);
        if (cx == e[0] && cz == e[1] && (e[2] & 0xF) == pose) break;
    }
    if (i == at::kArea108HookCellCount) return 0;
    if (Test(0x1C)) return 0;
    const unsigned char* const e = Mem(at::kArea108HookCells + i * 4);
    AH_CALL(Flags_Toggle)(StoryFlags(), e[3]);
    AH_CALL(Sound_PlayEffect)(0x206);
    const unsigned slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        Set(0x1C);
        unsigned char* const r = EffectAt(slot);
        r[0] = 1;
        r[5] = 0x18;
        r[0xB] = 0x1B;
        r[1] = 0;
        SetWord(r + 0x36, e[0]);
        SetWord(r + 0x3A, e[1]);
    }
    return 1;
}

// original 0x417210 (Field_ModeTailKinds[12], armed by Area108_PlaceScene):
// its jump table (10 entries) by the s8 state, unsigned above 9: nothing.
//   0: the timer down to 0: Field_Kind2X / Z = extra object 1's x / z,
//      MoveScript_F3Divisor = Field_MoveSpeeds[3] << 3, the next state.
//   1: Field_Kind2Hold 0: Kind2_Place(extra object 1's +0x83 - 1),
//      Sprite_Kind2 +0x84 = 4, counter 3 = 1, the state (read again) + 1.
//   2: counter 3 at 2: counter 3 = 0; extra object 1's +0x83 at 9: state 4;
//      else the divisor from Field_MoveSpeeds[4], Kind2 X / Z the leader's,
//      the next state.
//   3: Kind2Hold 0: MapView_SetElevation(AreaMap_Elevation(Kind2 X, Z)), the
//      tail disarmed (state, kind 0, then ScriptFlags_Clear40).
//   4: counter 3 at 4: an effect 0x35 at (0x218000, 0x2F8000) with its ground
//      (not shifted) if a slot is free; Area108_Model's flags byte & 0xF1 |
//      0x41; the timer 0x1E; state 5.
//   5: the timer down to 0: story flag 0x1D, MoveCmd_TestFB(0x20, 0x2F),
//      state 9 (the ebx it entered with), extra object 0's +0 = 0, the timer
//      0x1E. Else each of Area108_Model's records (its s8 count, read again
//      each pass) has its four corners moved by its velocity >> 8 (s16), and
//      extra object 0's +0x5D, +0x5F, +0x5E less 4.
//   6..8: nothing. 9: the timer down to 0: MoveScript_Var7 = 3, the state,
//      the kind and the run's step 0.
extern "C" void __cdecl Area108_TailPlace(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (static_cast<U>(static_cast<std::int32_t>(state)) > 9) return;
    switch (state) {
    case 0:
        if (DecTimer() != 0) return;
        Field_Kind2X = Long(Extra(1) + 0x34);
        Field_Kind2Z = Long(Extra(1) + 0x38);
        MoveScript_F3Divisor = static_cast<short>(B(at::kMoveSpeed3) << 3);
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    case 1:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(Kind2_Place)(static_cast<unsigned char>(Extra(1)[0x83] - 1));
        Sprite_Kind2[0x84] = 4;
        B(at::kCounter3) = 1;
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    case 2: {
        if (B(at::kCounter3) != 2) return;
        const unsigned char script = Extra(1)[0x83];
        B(at::kCounter3) = 0;
        if (script == 9) {
            B(at::kTailState) = 4;
            return;
        }
        MoveScript_F3Divisor = static_cast<short>(B(at::kMoveSpeed4) << 3);
        Field_Kind2X = Long(Mem(at::kLeaderX));
        Field_Kind2Z = Long(Mem(at::kLeaderZ));
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    }
    case 3:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(MapView_SetElevation)(AH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z));
        B(at::kTailState) = 0;
        B(at::kTailKind) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 4: {
        if (B(at::kCounter3) != 4) return;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            unsigned char* const e = EffectAt(slot);
            SetLong(e + 0x38, 0x2F8000);
            SetLong(e + 0x34, 0x218000);
            e[0] = 1;
            e[5] = 0x35;
            const long ground = AH_CALL(AreaMap_Elevation)(Long(e + 0x34), Long(e + 0x38));
            SetLong(e + 0x3C, static_cast<std::int16_t>(ground));
        }
        B(at::kArea108ModelFlags) = static_cast<unsigned char>((B(at::kArea108ModelFlags) & 0xF1) | 0x41);
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 5;
        return;
    }
    case 5: {
        if (DecTimer() == 0) {
            Set(0x1D);
            AH_CALL(MoveCmd_TestFB)(0x20, 0x2F);
            B(at::kTailState) = 9;
            Extra(0)[0] = 0;
            SetWord(Mem(at::kTailTimer), 0x1E);
            return;
        }
        if (static_cast<signed char>(B(at::kArea108Model)) > 0) {
            unsigned char* p = Ptr(at::kArea108ModelPoints) + 2;
            unsigned char i = 0;
            do {
                const auto vx = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x18)) >> 8);
                const auto vy = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x1A)) >> 8);
                const auto vz = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x1C)) >> 8);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6, Word(p + c * 6) + vx);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6 + 2, Word(p + c * 6 + 2) + vy);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6 + 4, Word(p + c * 6 + 4) + vz);
                p += at::kArea108ModelStride;
                ++i;
            } while (static_cast<std::int32_t>(i) < static_cast<signed char>(B(at::kArea108Model)));
        }
        unsigned char* const extra0 = Extra(0);
        extra0[0x5D] = static_cast<unsigned char>(extra0[0x5D] + 0xFC);
        extra0[0x5F] = static_cast<unsigned char>(extra0[0x5F] + 0xFC);
        extra0[0x5E] = static_cast<unsigned char>(extra0[0x5E] + 0xFC);
        return;
    }
    case 9:
        if (DecTimer() != 0) return;
        MoveScript_Var7 = 3;
        B(at::kTailState) = 0;
        B(at::kTailKind) = 0;
        B(at::kVar7Step) = 0;
        return;
    default: return;   // 6..8
    }
}

// original 0x417510 (called by Area108_PlaceScene and
// Area108_CellsIfRequest5): each of Area108_SetCells's four (x, z) words -
// on: AreaMap_SetHeight(x, z, 0xE), AreaMap_SetByte(x, z, 0x11); off: both
// 0.
extern "C" void __cdecl Area108_SetCells(unsigned char on) {
    for (unsigned i = 0; i < at::kArea108SetCellCount; ++i) {
        const unsigned char* const c = Mem(at::kArea108SetCells + i * 4);
        const unsigned x = Word(c);
        const unsigned z = Word(c + 2);
        AH_CALL(AreaMap_SetHeight)(x, z, on != 0 ? 0xEu : 0u);
        SetByte(x, z, on != 0 ? 0x11u : 0u);
    }
}

// ===========================================================================
// Area 110 (descriptor 0x61E808): its init.
// ===========================================================================

// original 0x417580 (area 110 +0x40; PSX 0x801F2D2C): a jmp over eleven nops
// to its body 0x417590, area 72's code (Area72_PlaceRandomObject) over
// Area110_Cells / Area110_Weights.
extern "C" void __cdecl Area110_PlaceRandomObject(void) { PlaceRandomObject(at::kArea110Cells, at::kArea110Weights); }

// ===========================================================================
// Area 111 (descriptor 0x61FB38): eleven choices (choices 2..10 are handlers
// 0..8), tail kind 17, the arrive hook, the init, two helpers. The handlers
// work a 7 x 7 grid of nibbles (Area111_Grid) over the map cells from x 0xB,
// z 0xB1, two map cells a grid cell.
// ===========================================================================

// original 0x417650 (area 111 +0x34[0]): the message word 3 for an answer,
// else 2.
extern "C" void __cdecl Area111_ChoiceMessage2(void) { SetMessage(B(at::kChoiceAnswer) != 0 ? 3u : 2u); }

// original 0x417670 (area 111 +0x34[1]): the message word 0xFFFF; answer 0:
// ScriptFlags_Set40, MoveScript_Var7 = 5, the run's step 0.
extern "C" void __cdecl Area111_ChoiceStartVar7(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = 5;
    B(at::kVar7Step) = 0;
}

// original 0x4176A0 (area 111 +0x3C[0]; PSX 0x801F3448): extra object 0's
// dword +0x6C = extra object 1's.
extern "C" void __cdecl Area111_CopyExtra6C(void) { SetLong(Extra(0) + 0x6C, Long(Extra(1) + 0x6C)); }

// original 0x4176B0 (area 111 +0x3C[1]; PSX 0x801F3460): the running
// object's grid cell (x, z high words less 0xB / 0xB1, halved rounding down)
// marked 0x10 (Area111_MarkCells); then from it, by the object's direction
// (the cell steps, re-read after the call), cells stepped while inside the
// grid (16-bit tests, 0..6) and their nibble 0, the count + 4 each (a byte);
// a count: the script object's +7 = count + 1. The object's dword +0x14 = 0,
// the cell it stopped at (outside or blocked) marked 0, Sound_PlayEffect(0x203).
extern "C" void __cdecl Area111_SlideMark(void) {
    const unsigned char* const cur = Sprite_Current;
    int z = Floor2(S16(cur + 0x3A) - 0xB1);
    int x = Floor2(S16(cur + 0x36) - 0xB);
    unsigned char count = 0;
    AH_CALL(Area111_MarkCells)(x, z, 0x10);
    const unsigned d = (Sprite_Current[8] & 7u) * 2;
    for (;;) {
        x += static_cast<signed char>(B(at::kCellDelta + d));
        if (static_cast<std::int16_t>(x) < 0 || static_cast<std::int16_t>(x) > 6) break;
        z += static_cast<signed char>(B(at::kCellDelta + d + 1));
        if (static_cast<std::int16_t>(z) < 0 || static_cast<std::int16_t>(z) > 6) break;
        if (Nibble(static_cast<unsigned>(static_cast<std::int16_t>(x)), static_cast<unsigned>(static_cast<std::int16_t>(z))) != 0) break;
        count = static_cast<unsigned char>(count + 4);
    }
    if (count != 0) MoveScript_Object[7] = static_cast<unsigned char>(count + 1);
    SetLong(Sprite_Current + 0x14, 0);
    AH_CALL(Area111_MarkCells)(x, z, 0);
    AH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x417790 (area 111 +0x3C[2]; PSX 0x801F35F4): the grid's nibble at
// the leader's cell (bytes, unchecked; inside .data), less 1; 0xFF (an empty
// cell): nothing. Else MoveCmd_Attach(the script object, 7, that, Field_State's
// slot + 1, 0x10) with the script position kept across the call; the running
// object's direction = extra object that's +8 & 7, and Sprite_Kind2's +8 =
// it.
extern "C" void __cdecl Area111_AttachAtLeaderCell(void) {
    const Cell c = LeaderCell();
    const auto block = static_cast<unsigned char>(Nibble(c.col, c.row) - 1);
    if (block == 0xFF) return;
    const unsigned char slot = FieldStateSlot();
    unsigned char* const object = MoveScript_Object;
    const std::uint16_t position = Word(object + 0xA);
    AH_CALL(MoveCmd_Attach)(object, 7, block, slot, 0x10);
    SetWord(MoveScript_Object + 0xA, position);
    Sprite_Current[8] = static_cast<unsigned char>(Extra(block)[8] & 7);
    Sprite_Kind2[8] = Sprite_Current[8];
}

// original 0x417890 (area 111 +0x3C[3]; PSX 0x801F3768): counter 3 = 0x10 <<
// the running object's dword +0x18 (a byte shift, the count masked to 5
// bits).
extern "C" void __cdecl Area111_Counter3Bit(void) {
    const unsigned shift = static_cast<U>(Long(Sprite_Current + 0x18)) & 31;
    B(at::kCounter3) = static_cast<unsigned char>(0x10u << shift);
}

// original 0x4178B0 (area 111 +0x3C[4]; PSX 0x801F3790): unless counter 3 is
// 0x10 << the active member's index among the extra objects (a dword shift,
// the count masked to 5 bits), the script position - 2.
extern "C" void __cdecl Area111_WaitMemberBit(void) {
    const unsigned shift = static_cast<U>(ActiveMemberExtra()) & 31;
    if (static_cast<U>(B(at::kCounter3)) != (0x10u << shift)) ScriptStep(-2);
}

// original 0x4178F0 (area 111 +0x3C[5]; PSX 0x801F3808): the key = the active
// member's extra index + 1 (a byte). The running object's cell (x, z high
// words less 0xB / 0xB1, halved toward 0) marked 0 (Area111_MarkCells); every
// nibble of the grid holding the key cleared (each byte seen once for each of
// its two columns: its low nibble first); the key or'd into the object's
// cell's nibble; Sound_PlayEffect(0x204). The cell is not checked: ours
// aborts where the byte written would lie outside the 28-byte grid.
extern "C" void __cdecl Area111_PlaceMemberInGrid(void) {
    const auto key = static_cast<unsigned char>(ActiveMemberExtra() + 1);
    const unsigned char* const cur = Sprite_Current;
    const int x = Half(S16(cur + 0x36) - 0xB);
    const int z = Half(S16(cur + 0x3A) - 0xB1);
    AH_CALL(Area111_MarkCells)(x, z, 0);
    for (unsigned row = 0; row < at::kArea111GridSide; ++row) {
        for (unsigned col = 0; col < at::kArea111GridSide; ++col) {
            unsigned char& b = B(at::kArea111Grid + (col >> 1) + row * 4);
            if ((b & 0xF) == key)
                b = static_cast<unsigned char>(b & 0xF0);
            else if ((b >> 4) == key)
                b = static_cast<unsigned char>(b & 0xF);
        }
    }
    const int offset = Half(static_cast<std::int16_t>(x)) + static_cast<std::int16_t>(z) * 4;
    if (offset < 0 || offset >= static_cast<int>(at::kArea111GridBytes))
        bof3::Fatal("Area111_PlaceMemberInGrid: the cell (%d, %d) lies outside the 7 x 7 grid 0x%X", x, z,
                    static_cast<unsigned>(at::kArea111Grid));
    unsigned char& b = B(at::kArea111Grid + static_cast<U>(offset));
    b = static_cast<unsigned char>(b | ((x & 1) != 0 ? key : static_cast<unsigned char>(key << 4)));
    AH_CALL(Sound_PlayEffect)(0x204);
}

// original 0x4179F0 (area 111 +0x3C[6]; PSX 0x801F39E0): extra object k's
// (k the running object's byte +0x18, unchecked; inside .data) cell, halved
// toward 0, stepped once by the running object's direction (bytes); outside
// 0..6 (unsigned): nothing. Its nibble less 1 at 4 or more: nothing. Else
// MoveCmd_Attach(the script object, 7, that block, Field_State's slot + 1,
// 0xC) with the script position kept across the call; extra object block's +8
// = the running object's direction & 7; the script position - 6.
extern "C" void __cdecl Area111_AttachAhead(void) {
    const unsigned char* const cur = Sprite_Current;
    const unsigned char k = cur[0x18];
    const unsigned d = (cur[8] & 7u) * 2;
    const unsigned char* const extra = Extra(k);
    const auto col = static_cast<unsigned char>(Half(S16(extra + 0x36) - 0xB) + B(at::kCellDelta + d));
    const auto row = static_cast<unsigned char>(Half(S16(extra + 0x3A) - 0xB1) + B(at::kCellDelta + d + 1));
    if (col >= 7 || row >= 7) return;
    const auto block = static_cast<unsigned char>(Nibble(col, row) - 1);
    if (block >= 4) return;
    const unsigned char slot = FieldStateSlot();
    unsigned char* const object = MoveScript_Object;
    const std::uint16_t position = Word(object + 0xA);
    AH_CALL(MoveCmd_Attach)(object, 7, block, slot, 0xC);
    SetWord(MoveScript_Object + 0xA, position);
    Extra(block)[8] = static_cast<unsigned char>(Sprite_Current[8] & 7);
    ScriptStep(-6);
}

// original 0x417B30 (area 111 +0x3C[7]; PSX 0x801F3BC0): from the running
// object's cell (the 16-bit high words less 0xB / 0xB1, shifted right once:
// bytes) cells stepped by its direction while inside the grid (signed bytes,
// 0..6) and their nibble not 0xF, the count + 4 each. A count: the script
// object's +7 = count + 1, MoveCmd_MoveKind2(the object's direction byte),
// MoveScript_FAWord 0, the script object's +0 bit 0x40, the running object's
// dword +0x14 = 0 and word +0x3E = 0xFB00.
extern "C" void __cdecl Area111_SlideAhead(void) {
    const unsigned char* const cur = Sprite_Current;
    auto col = static_cast<unsigned char>(static_cast<std::uint16_t>(Word(cur + 0x36) - 0xB) >> 1);
    auto row = static_cast<unsigned char>(static_cast<std::uint16_t>(Word(cur + 0x3A) - 0xB1) >> 1);
    const unsigned d = (cur[8] & 7u) * 2;
    const unsigned char step = B(at::kCellDelta + d);
    unsigned char count = 0;
    for (;;) {
        col = static_cast<unsigned char>(col + step);
        if (static_cast<signed char>(col) < 0 || static_cast<signed char>(col) > 6) break;
        row = static_cast<unsigned char>(row + B(at::kCellDelta + d + 1));
        if (static_cast<signed char>(row) < 0 || static_cast<signed char>(row) > 6) break;
        if (Nibble(col, row) == 0xF) break;
        count = static_cast<unsigned char>(count + 4);
    }
    if (count == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(count + 1);
    AH_CALL(MoveCmd_MoveKind2)(Sprite_Current[8]);
    MoveScript_FAWord = 0;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    SetLong(Sprite_Current + 0x14, 0);
    SetWord(Sprite_Current + 0x3E, 0xFB00);
}

// original 0x417C10 (area 111 +0x3C[8]; PSX 0x801F3D3C): extra object k's
// (k the running object's dword +0x18) x and z, their high words stepped by
// twice the cell step of the object's direction; the ground there less the
// object's word +0x3E (16 bits). With the script object's +4 set: the object's
// +9 = 0x20 and its dwords +0xC, +0x10, +0x14 = the x, z and height
// differences / 32 (signed, toward 0). k is not checked: ours aborts where
// the record would lie outside the image's .data.
extern "C" void __cdecl Area111_StepToExtra(void) {
    const unsigned char* const cur = Sprite_Current;
    const U k = static_cast<U>(Long(cur + 0x18));
    const U record = at::kSpriteObjectsExtra + k * at::kExtraStride;
    if (record < at::kDataStart || record > at::kDataEnd - 0x3C)
        bof3::Fatal("Area111_StepToExtra: extra object %u lies at 0x%X, outside the image's .data", k, record);
    const unsigned char* const extra = Mem(record);
    const U z0 = static_cast<U>(Long(cur + 0x38));
    U x = static_cast<U>(Long(extra + 0x34));
    U z = static_cast<U>(Long(extra + 0x38));
    const unsigned d = (cur[8] & 7u) * 2;
    const auto sx = static_cast<std::uint16_t>(static_cast<signed char>(B(at::kCellDelta + d)) * 2);
    const auto sz = static_cast<std::uint16_t>(static_cast<signed char>(B(at::kCellDelta + d + 1)) * 2);
    x = (x & 0xFFFFu) | static_cast<U>(static_cast<std::uint16_t>((x >> 16) + sx)) << 16;
    z = (z & 0xFFFFu) | static_cast<U>(static_cast<std::uint16_t>((z >> 16) + sz)) << 16;
    const auto dx = static_cast<std::int32_t>(x - static_cast<U>(Long(cur + 0x34)));
    const auto dz = static_cast<std::int32_t>(z - z0);
    const long ground = AH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
    const auto dy = static_cast<std::int16_t>(static_cast<std::uint16_t>(ground) - Word(Sprite_Current + 0x3E));
    if (MoveScript_Object[4] == 0) return;
    Sprite_Current[9] = 0x20;
    SetLong(Sprite_Current + 0xC, dx / 32);
    SetLong(Sprite_Current + 0x10, dz / 32);
    SetLong(Sprite_Current + 0x14, static_cast<std::int32_t>(dy) / 32);
}

// original 0x417CF0 (Field_ModeTailKinds[17], armed by Area111_ArriveHook and
// Area111_ArmTailAtLeaderCell): the s8 state less 1 through the byte table
// 0x417EB8 (24 entries) to the jump table 0x417E98 (8), unsigned above 0x17:
// nothing.
//   1, 3, 5, 7: Party_DropIn(0), the timer and the state 0xA.
//   0xA: the timer down to 0: kind, state and sub-kind 0, ScriptFlags_Clear40.
//   0x14: MoveCmd_TestFB(0x11, 0xB7), the timer 0x1E, state 0x15.
//   0x15: the timer down to 0: the divisor from Field_MoveSpeeds[4], Kind2 X /
//         Z = (0x118000, 0xAF0000), MoveScript_FAWord 0, state 0x16.
//   0x16: Field_Kind2Hold 0: MoveCmd_TestFB(0x11, 0xAD), the map bytes (0x11,
//         0xAD), (0x12, 0xAD) = 0xA1 and (0x11, 0xAE), (0x12, 0xAE) = 0xC0,
//         Sound_PlayEffect(0x205), story flag 0x28, the timer 0x1E, state 0x17.
//   0x17: the timer down to 0: the divisor from speed 4, Kind2 X / Z the
//         leader's, FAWord 0, state 0x18.
//   0x18: Kind2Hold 0: the timer 1, state 0xA.
//   Any other: nothing.
extern "C" void __cdecl Area111_TailGate(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 1: case 3: case 5: case 7:
        AH_CALL(Party_DropIn)(0);
        SetWord(Mem(at::kTailTimer), 0xA);
        B(at::kTailState) = 0xA;
        return;
    case 0xA:
        if (DecTimer() != 0) return;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kTailSub) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 0x14:
        AH_CALL(MoveCmd_TestFB)(0x11, 0xB7);
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x15;
        return;
    case 0x15:
        if (DecTimer() != 0) return;
        Field_Kind2X = 0x118000;
        Field_Kind2Z = 0xAF0000;
        MoveScript_F3Divisor = static_cast<short>(B(at::kMoveSpeed4) << 3);
        MoveScript_FAWord = 0;
        B(at::kTailState) = 0x16;
        return;
    case 0x16:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(MoveCmd_TestFB)(0x11, 0xAD);
        SetByte(0x11, 0xAD, 0xA1);
        SetByte(0x12, 0xAD, 0xA1);
        SetByte(0x11, 0xAE, 0xC0);
        SetByte(0x12, 0xAE, 0xC0);
        AH_CALL(Sound_PlayEffect)(0x205);
        Set(0x28);
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x17;
        return;
    case 0x17:
        if (DecTimer() != 0) return;
        Field_Kind2X = Long(Mem(at::kLeaderX));
        Field_Kind2Z = Long(Mem(at::kLeaderZ));
        MoveScript_F3Divisor = static_cast<short>(B(at::kMoveSpeed4) << 3);
        MoveScript_FAWord = 0;
        B(at::kTailState) = 0x18;
        return;
    case 0x18:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(at::kTailTimer), 1);
        B(at::kTailState) = 0xA;
        return;
    default: return;
    }
}

namespace {
// Area 111's arrive hook's edge tests: the half-cell high word `a` (the
// coordinate the edge lies on) at `at`, the other `b` within [lo, lo + span)
// (16 bits), and the cell step of the leader's direction along a (x: +0, z:
// +1) such that at - step is `from`.
bool Edge(std::int16_t a, int at, std::int16_t b, unsigned lo, unsigned span, unsigned step_offset, int from, const unsigned char* cur) {
    if (a != at) return false;
    if (static_cast<std::uint16_t>(b - lo) >= span) return false;
    const unsigned d = (cur[8] & 7u) * 2 + step_offset;
    return at - static_cast<signed char>(B(at::kCellDelta + d)) == from;
}
}  // namespace

// original 0x417ED0 (Area_ArriveHook's case for area 111, 0x56E53B): with
// Cond_ByteFD 0, story flag 0x28 cleared; unless Cond_ByteFD (read again) is
// 1, al 0. The cell in half cells (x >> 15, z >> 15, 16 bits) on one of the
// grid's four edges, arriving across it (the leader's direction's step):
// Area111_ArmTailAtLeaderCell(3 / 7 / 5 / 1 by the edge), al 0; the gates
// (x = 0x20 / 0x26, z = 0x16C / 0x172 over three cells) likewise; else the
// cell (x high word 0x11..0x12, z 0xB7..0xB8) with flag 0x28 clear:
// ScriptFlags_Set40, tail kind 0x11, state 0x14. al 0 always.
extern "C" unsigned char __cdecl Area111_ArriveHook(long x, long z) {
    if (Cond_ByteFD == 0) Clear(0x28);
    if (Cond_ByteFD != 1) return 0;
    const unsigned char* const cur = Sprite_Current;
    const auto hx = static_cast<std::int16_t>(static_cast<std::int32_t>(x) >> 15);
    const auto hz = static_cast<std::int16_t>(static_cast<std::int32_t>(z) >> 15);
    const auto arm = [](unsigned char state) { AH_CALL(Area111_ArmTailAtLeaderCell)(state); };
    // x edges (0x16, 0x30) over z 0x162..0x17C
    if (Edge(hx, 0x16, hz, 0x162, 0x1B, 0, 0x15, cur)) {
        arm(3);
        return 0;
    }
    if (Edge(hx, 0x30, hz, 0x162, 0x1B, 0, 0x31, cur)) {
        arm(7);
        return 0;
    }
    // z edges (0x162, 0x17C) over x 0x16..0x30
    if (Edge(hz, 0x162, hx, 0x16, 0x1B, 1, 0x161, cur)) {
        arm(5);
        return 0;
    }
    if (Edge(hz, 0x17C, hx, 0x16, 0x1B, 1, 0x17D, cur)) {
        arm(1);
        return 0;
    }
    // the gates: x 0x20 / 0x26 over z 0x16E..0x170
    if (Edge(hx, 0x20, hz, 0x16E, 3, 0, 0x21, cur)) {
        arm(7);
        return 0;
    }
    if (Edge(hx, 0x26, hz, 0x16E, 3, 0, 0x25, cur)) {
        arm(3);
        return 0;
    }
    // z 0x16C / 0x172 over x 0x22..0x24
    if (Edge(hz, 0x16C, hx, 0x22, 3, 1, 0x16D, cur)) {
        arm(1);
        return 0;
    }
    if (Edge(hz, 0x172, hx, 0x22, 3, 1, 0x171, cur)) {
        arm(5);
        return 0;
    }
    const auto xh = static_cast<std::uint16_t>(static_cast<U>(x) >> 16);
    const auto zh = static_cast<std::uint16_t>(static_cast<U>(z) >> 16);
    if (static_cast<std::uint16_t>(xh - 0x11) < 2 && static_cast<std::uint16_t>(zh - 0xB7) < 2 && !Test(0x28)) {
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0x11;
        B(at::kTailState) = 0x14;
    }
    return 0;
}

// original 0x418130 (called by Area111_ArriveHook): the grid's nibble at the
// leader's cell less 1 to the sub-kind; tail kind 0x11, the state `state`;
// extra object (that byte)'s +8 = state (unchecked: a nibble of 0 writes
// record 0xFF, inside .data); ScriptFlags_Set40.
extern "C" void __cdecl Area111_ArmTailAtLeaderCell(unsigned char state) {
    const Cell c = LeaderCell();
    const auto block = static_cast<unsigned char>(Nibble(c.col, c.row) - 1);
    B(at::kTailSub) = block;
    B(at::kTailKind) = 0x11;
    B(at::kTailState) = state;
    Extra(block)[8] = state;
    AH_CALL(ScriptFlags_Set40)();
}

// original 0x4181E0 (called by Area111_SlideMark, Area111_PlaceMemberInGrid,
// Area111_Init): the grid cell (x, z)'s four map bytes (2x + 0xB..0xC, 2z +
// 0xB1..0xB2) set to `value`.
extern "C" void __cdecl Area111_MarkCells(int x, int z, unsigned value) {
    const auto x0 = static_cast<unsigned>(x + x + 0xB);
    const auto x1 = static_cast<unsigned>(x + x + 0xC);
    const auto z0 = static_cast<unsigned>(z + z + 0xB1);
    const auto z1 = static_cast<unsigned>(z + z + 0xB2);
    SetByte(x0, z0, value);
    SetByte(x1, z0, value);
    SetByte(x0, z1, value);
    SetByte(x1, z1, value);
}

// original 0x418240 (area 111 +0x40; PSX 0x801F46A8): with Cond_ByteFD 0,
// story flag 0x28 cleared. Cond_ByteFD (read again) 1: every grid cell marked
// with its byte of Area111_CellBytes (rows of z, columns of x), the grid
// copied from Area111_GridStart, and the gate's map bytes (0x11..0x12, 0xAD)
// = 0x50 and (.., 0xAE) = 0 without flag 0x28, 0xA1 and 0xC0 with it.
// Cond_ByteFD 5: the map bytes (0xA..0xB, 8) = 0x50.
extern "C" void __cdecl Area111_Init(void) {
    if (Cond_ByteFD == 0) Clear(0x28);
    if (Cond_ByteFD == 1) {
        const unsigned char* bytes = Mem(at::kArea111CellBytes);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 7; ++col) AH_CALL(Area111_MarkCells)(col, row, *bytes++);
        for (unsigned i = 0; i < at::kArea111GridBytes; ++i) B(at::kArea111Grid + i) = B(at::kArea111GridStart + i);
        if (!Test(0x28)) {
            SetByte(0x11, 0xAD, 0x50);
            SetByte(0x12, 0xAD, 0x50);
            SetByte(0x11, 0xAE, 0);
            SetByte(0x12, 0xAE, 0);
        } else {
            SetByte(0x11, 0xAD, 0xA1);
            SetByte(0x12, 0xAD, 0xA1);
            SetByte(0x11, 0xAE, 0xC0);
            SetByte(0x12, 0xAE, 0xC0);
        }
    }
    if (Cond_ByteFD == 5) {
        SetByte(0xA, 8, 0x50);
        SetByte(0xB, 8, 0x50);
    }
}

// ===========================================================================
// Area 112 (descriptor 0x61FF30): three choices (handlers 3..5), handlers 0
// and 2 (handler 1 is 0x421FB0, another block's), the step and cell hooks,
// the cell hook's helper, and effect kind 0xB9's handler with its ring state
// (its state 0 is area 59's Area59_EffectGround).
// ===========================================================================

// original 0x418350 (area 112 +0x3C[0]; PSX 0x801F3558): Field_ChangeArea by
// story flags 0x34 (2) and 0x35 (1): 1 - (0x8F, 0x388000, 0x650000, 0x81); 2
// - (0x92, 0x458000, 0x2B0000, 0x81); 3 - (0x64, 0x468000, 0x340000, 0x81);
// neither - (0x91, 0xA8000, 0x630000, 0x84).
extern "C" void __cdecl Area112_ChangeAreaByFlags(void) {
    const unsigned low = Test(0x35) ? 1u : 0u;
    const unsigned high = Test(0x34) ? 2u : 0u;
    switch (high | low) {
    case 1: AH_CALL(Field_ChangeArea)(0x8F, 0x388000, 0x650000, 0x81); return;
    case 2: AH_CALL(Field_ChangeArea)(0x92, 0x458000, 0x2B0000, 0x81); return;
    case 3: AH_CALL(Field_ChangeArea)(0x64, 0x468000, 0x340000, 0x81); return;
    default: AH_CALL(Field_ChangeArea)(0x91, 0xA8000, 0x630000, 0x84); return;
    }
}

// original 0x418400 (area 112 +0x3C[2]; PSX 0x801F366C): the first of
// Area112_MemberKeys that some member (0..Field_MemberCount - 1, the count
// read once) has as its +0x89: Msg_OpenScript(Area112_Messages[that]),
// Field_Request 2. None: nothing.
extern "C" void __cdecl Area112_TalkByMember(void) {
    const unsigned count = Field_MemberCount;
    for (unsigned k = 0; k < at::kArea112KeyCount; ++k) {
        const unsigned char key = B(at::kArea112MemberKeys + k);
        for (unsigned m = 0; m < count; ++m) {
            if (Record(m)[0x89] != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(at::kArea112Messages + k * 2)));
            Field_Request = 2;
            return;
        }
    }
}

// original 0x418490 (area 112 +0x34[0] = +0x3C[3]; PSX 0x801F3730): the
// message word the s8 answer + 3; story flag 0x33 set; by the answer (read
// again): 1 - 0x34 cleared, 0x35 set; 2 - 0x34 set, 0x35 cleared; 3 - both
// set; 4 - the message 0xFFFF; any other - both cleared. Then without Cond row
// 13's flag 0x10: the message 2 and flag 0x33 cleared.
extern "C" void __cdecl Area112_ChoiceFlags34(void) {
    SetMessage(static_cast<std::uint16_t>(static_cast<signed char>(B(at::kChoiceAnswer)) + 3));
    Set(0x33);
    switch (static_cast<signed char>(B(at::kChoiceAnswer))) {
    case 1:
        Clear(0x34);
        Set(0x35);
        break;
    case 2:
        Set(0x34);
        Clear(0x35);
        break;
    case 3:
        Set(0x34);
        Set(0x35);
        break;
    case 4: SetMessage(0xFFFF); break;
    default:
        Clear(0x34);
        Clear(0x35);
        break;
    }
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow13), 0x10) != 0) return;
    SetMessage(2);
    Clear(0x33);
}

// original 0x418570 (area 112 +0x34[1] = +0x3C[4]; PSX 0x801F3870): the
// message word 0x14 for an answer, else 0x13.
extern "C" void __cdecl Area112_ChoiceMessage13(void) { SetMessage(B(at::kChoiceAnswer) != 0 ? 0x14u : 0x13u); }

// original 0x418590 (area 112 +0x34[2] = +0x3C[5]; PSX 0x801F3898): the
// message word 0xFFFF; answer 0: story flags 0x3C..0x3F and 0x38..0x3B
// cleared (in that order), Sound_PlayEffect(0x200).
extern "C" void __cdecl Area112_ChoiceClearFlags(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    for (unsigned f = 0x3C; f <= 0x3F; ++f) Clear(f);
    for (unsigned f = 0x38; f <= 0x3B; ++f) Clear(f);
    AH_CALL(Sound_PlayEffect)(0x200);
}

// original 0x418620 (Area_StepHook's case for area 112, 0x56E142):
// Cond_ByteFD 3, story flag 0x33, Cond row 13's flag 0x10, the x high word
// 0xB..0xD and z 0x57..0x59 (16 bits), the leader's pose 0, 7 or 6: counter 0
// = 0, Party_DropIn(0), al 1. Else al 0.
extern "C" unsigned char __cdecl Area112_StepHook(long x, long z) {
    if (Cond_ByteFD != 3) return 0;
    if (!Test(0x33)) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow13), 0x10) == 0) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(x) >> 16) - 0xB) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(z) >> 16) - 0x57) >= 3) return 0;
    const unsigned char pose = B(at::kLeaderPose);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(at::kCounter0) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x4186A0 (area 112's cell hook, Area_CellHooks): the first of
// Area112_HookCells whose x and z bytes are the cell's while the leader's pose
// byte is not 3; none, story flag 0x1C, or the entry's flag set: al 0.
// Area112_MemberNearBoxes: ScriptFlags_Set40, tail kind 0xA, state 0xA,
// sub-kind 0x15, al 0. Else each set bit of the entry's bits byte - bits 0..3
// clear, bits 4..7 set Area112_HookFlags[bit & 3] - then the entry's flag
// set, Sound_PlayEffect(0x200), Effect_HoldFlag1C(0xF), al 1.
extern "C" unsigned char __cdecl Area112_CellHook(long x, long z) {
    const auto cx = static_cast<unsigned char>(x);
    const auto cz = static_cast<unsigned char>(z);
    const unsigned char pose = B(at::kLeaderPose);
    unsigned i = 0;
    for (; i < at::kArea112HookCellCount; ++i) {
        const unsigned char* const e = Mem(at::kArea112HookCells + i * 4);
        if (cx == e[0] && cz == e[1] && pose != 3) break;
    }
    if (i == at::kArea112HookCellCount) return 0;
    if (Test(0x1C)) return 0;
    const unsigned char* const e = Mem(at::kArea112HookCells + i * 4);
    if (Test(e[3])) return 0;
    if (AH_CALL(Area112_MemberNearBoxes)() != 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0xA;
        B(at::kTailState) = 0xA;
        B(at::kTailSub) = 0x15;
        return 0;
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (((e[2] >> bit) & 1) == 0) continue;
        const unsigned char flag = B(at::kArea112HookFlags + (bit & 3));
        if (bit < 4)
            Clear(flag);
        else
            Set(flag);
    }
    Set(e[3]);
    AH_CALL(Sound_PlayEffect)(0x200);
    AH_CALL(Effect_HoldFlag1C)(0xF);
    return 1;
}

// original 0x4187C0 (called by Area112_CellHook): for each member
// (0..Field_MemberCount - 1) its next position (x +0x34 + the step +0xC times
// the frames +9, z +0x38 + +0x10 times +9, wrapping) against the four
// Area112_Boxes: |x >> 16 - centre x| below the half width and |z >> 16 -
// centre z| below the half depth (signed; the absolute value as cdq / xor /
// sub): al 1. None: al 0.
extern "C" unsigned char __cdecl Area112_MemberNearBoxes(void) {
    const unsigned count = Field_MemberCount;
    for (unsigned m = 0; m < count; ++m) {
        const unsigned char* const r = Record(m);
        const U n = r[9];
        const U px = static_cast<U>(Long(r + 0xC)) * n + static_cast<U>(Long(r + 0x34));
        const U pz = static_cast<U>(Long(r + 0x10)) * n + static_cast<U>(Long(r + 0x38));
        const std::int32_t hx = static_cast<std::int32_t>(px) >> 16;
        const std::int32_t hz = static_cast<std::int32_t>(pz) >> 16;
        for (unsigned b = 0; b < at::kArea112BoxCount; ++b) {
            const unsigned char* const box = Mem(at::kArea112Boxes + b * 4);
            const auto abs32 = [](std::int32_t v) {
                const U s = static_cast<U>(v >> 31);
                return static_cast<std::int32_t>((static_cast<U>(v) ^ s) - s);
            };
            if (abs32(static_cast<std::int32_t>(static_cast<U>(hx) - box[0])) >= static_cast<std::int32_t>(box[2])) continue;
            if (abs32(static_cast<std::int32_t>(static_cast<U>(hz) - box[1])) < static_cast<std::int32_t>(box[3])) return 1;
        }
    }
    return 0;
}

// original 0x418860 (Effect_KindHandlers entry at 0x655618; a gap of the
// tool): Area112_EffectStates by Sprite_Current[1] (the running effect
// record).
extern "C" void __cdecl Area112_EffectRun(void) {
    StateEntry("Area112_EffectRun", at::kArea112EffectStates, at::kArea112EffectStateCount, Sprite_Current[1])();
}

// original 0x418880 (Area112_EffectStates[1]): Area100_EffectB7Ring's code -
// the point (x +0x34, z +0x38, y +0x3C) of the running record copied to the
// stack and handed to 0x4220D0 (area_w2f_callees.h).
extern "C" void __cdecl Area112_EffectRing(void) {
    const unsigned char* const cur = Sprite_Current;
    long point[4];
    point[0] = Long(cur + 0x34);
    point[1] = Long(cur + 0x38);
    point[2] = Long(cur + 0x3C);
    point[3] = 0;
    AH_AT(void (__cdecl*)(const long*), at::kRingAt)(point);
}

// ===========================================================================
// Area 113 (descriptor 0x620010): five choices (choices 1, 2 are 0x420850 /
// 0x420870, another block's; 3, 4 area 61's Area61_ChoiceMark4), object
// triggers 27, 39 and 63, tail kind 62 and its reward helper. No handlers.
// ===========================================================================

// original 0x4188B0 (area 113 +0x34[0]): an answer: the message word 4 and
// the mark 6. Answer 0: the byte 0x9045F4 at 6 or more: the message 2; below:
// the message 3 and the mark 6.
extern "C" void __cdecl Area113_ChoiceAsk(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(4);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (B(at::kByte9045F4) >= 6) {
        SetMessage(2);
        return;
    }
    SetMessage(3);
    B(at::kAnswerMark) = 6;
}

// original 0x4188F0 (Field_ObjectTriggers id 27; a gap of the tool):
// ScriptFlags_Set40, tail kind 4 with sub-kind 6 (the state not written); al
// 0.
extern "C" unsigned char __cdecl Area113_Trigger27(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 6;
    return 0;
}

// original 0x418910 (Field_ObjectTriggers id 39; a gap of the tool):
// ScriptFlags_Set40, tail kind 0x2C, state 0, sub-kind 0xD; al 0.
extern "C" unsigned char __cdecl Area113_Trigger39(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0xD;
    return 0;
}

// original 0x418930 (Field_ObjectTriggers id 63; a gap of the tool):
// ScriptFlags_Set40, tail kind 0x3E (Area113_TailReward), state 0, the
// object's word +0x88 = 5; al 0.
extern "C" unsigned char __cdecl Area113_Trigger63(unsigned char* object, unsigned char* flags) {
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x3E;
    B(at::kTailState) = 0;
    SetWord(object + 0x88, 5);
    return 0;
}

// original 0x418960 (Field_ModeTailKinds[62], armed by Area113_Trigger63; a
// gap of the tool): the s8 state through the byte table 0x418A34 (12
// entries) to the jump table 0x418A1C (6), unsigned above 0xB: nothing.
//   0: the state = Area113_Reward's answer.
//   1: Field_Request not 2: ScriptFlags_Clear40, kind and state 0.
//   5: Field_Request not 2: Sound_PlayEffect(0x106), ScriptFlags_Clear40,
//      kind and state 0.
//   0xA: Field_Request not 2: Sound_StopMusic, Sound_LoadStream(2), state 0xB.
//   0xB: the stream done and Field_Request not 2: Sound_ResumeAll,
//      ScriptFlags_Clear40, kind and state 0, the focus object's word +0x88 =
//      9.
//   2..4, 6..9: nothing.
extern "C" void __cdecl Area113_TailReward(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0: B(at::kTailState) = AH_CALL(Area113_Reward)(); return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 5:
        if (Field_Request == 2) return;
        AH_CALL(Sound_PlayEffect)(0x106);
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0xA:
        if (Field_Request == 2) return;
        AH_CALL(Sound_StopMusic)();
        AH_CALL(Sound_LoadStream)(2);
        B(at::kTailState) = 0xB;
        return;
    case 0xB: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        if (Field_Request == 2) return;
        AH_CALL(Sound_ResumeAll)();
        AH_CALL(ScriptFlags_Clear40)();
        unsigned char* const focus = Ptr(at::kFocusObject);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        SetWord(focus + 0x88, 9);
        return;
    }
    default: return;
    }
}

// original 0x418A40 (called by Area113_TailReward): by the byte 0x9045F4 -
//   0xA: story flag 0x96 set: message 5, answer 1; else Inventory_Add(3,
//        0x2D, 3), flag 0x96, message 2, answer 5.
//   0xB: flag 0x97 set: 5 / 1; else Inventory_Add(3, 0x33, 1), flag 0x97,
//        then flag 0x96 set: message 6, else Inventory_Add(3, 0x2D, 3), flag
//        0x96, message 3; answer 5.
//   0xC: key item 0xF held: 5 / 1; else KeyItem_Add(0xF), then flag 0x97
//        set: message 8; else item 0x33 and flag 0x97, then flag 0x96 set:
//        message 7, else item 0x2D and flag 0x96, message 4; answer 0xA.
//   Any other: message 1, answer 1.
// Then Msg_OpenScript(the message), Field_Request 2; al the answer. (The
// originals push a fourth word, 0, to Inventory_Add, which takes three.)
extern "C" unsigned char __cdecl Area113_Reward(void) {
    unsigned message = 1;
    unsigned char answer = 1;
    switch (B(at::kByte9045F4)) {
    case 0xA:
        if (Test(0x96)) {
            message = 5;
            break;
        }
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 2;
        answer = 5;
        break;
    case 0xB:
        if (Test(0x97)) {
            message = 5;
            break;
        }
        AH_CALL(Inventory_Add)(3, 0x33, 1);
        Set(0x97);
        answer = 5;
        message = 6;
        if (Test(0x96)) break;
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 3;
        break;
    case 0xC:
        if (AH_CALL(KeyItem_Has)(0xF) != 0) {
            message = 5;
            break;
        }
        AH_CALL(KeyItem_Add)(0xF);
        answer = 0xA;
        message = 8;
        if (Test(0x97)) break;
        AH_CALL(Inventory_Add)(3, 0x33, 1);
        Set(0x97);
        message = 7;
        if (Test(0x96)) break;
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 4;
        break;
    default: break;
    }
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
    Field_Request = 2;
    return answer;
}

void AreaW2f_Inject() {
    if (bof3::WantsShadow("area_w2f")) area_w2f::SelfTest();
    BOF3_INJECT(Area108_ChoiceArmTail5);
    BOF3_INJECT(Area108_ChoiceArmTailA);
    BOF3_INJECT(Area108_ClearCounter1Bits0);
    BOF3_INJECT(Area108_ClearCounter1Bits2);
    BOF3_INJECT(Area108_ClearCounter1Bits4);
    BOF3_INJECT(Area108_ClearCounter1Bits6);
    BOF3_INJECT(Area108_PlaceScene);
    BOF3_INJECT(Area108_PlaceAtCellA);
    BOF3_INJECT(Area108_PlaceAtCellB);
    BOF3_INJECT(Area108_ScriptByFlags1E);
    BOF3_INJECT(Area108_FlagIfEffectState5);
    BOF3_INJECT(Area108_FadeRun);
    BOF3_INJECT(Area108_FadeBegin);
    BOF3_INJECT(Area108_FadeStep);
    BOF3_INJECT(Area108_FindEffectSlot);
    BOF3_INJECT(Area108_SpawnEffect51);
    BOF3_INJECT(Area108_CellsIfRequest5);
    BOF3_INJECT(Area108_CellHook);
    BOF3_INJECT(Area108_TailPlace);
    BOF3_INJECT(Area108_SetCells);
    BOF3_INJECT(Area110_PlaceRandomObject);
    BOF3_INJECT(Area111_ChoiceMessage2);
    BOF3_INJECT(Area111_ChoiceStartVar7);
    BOF3_INJECT(Area111_CopyExtra6C);
    BOF3_INJECT(Area111_SlideMark);
    BOF3_INJECT(Area111_AttachAtLeaderCell);
    BOF3_INJECT(Area111_Counter3Bit);
    BOF3_INJECT(Area111_WaitMemberBit);
    BOF3_INJECT(Area111_PlaceMemberInGrid);
    BOF3_INJECT(Area111_AttachAhead);
    BOF3_INJECT(Area111_SlideAhead);
    BOF3_INJECT(Area111_StepToExtra);
    BOF3_INJECT(Area111_TailGate);
    BOF3_INJECT(Area111_ArriveHook);
    BOF3_INJECT(Area111_ArmTailAtLeaderCell);
    BOF3_INJECT(Area111_MarkCells);
    BOF3_INJECT(Area111_Init);
    BOF3_INJECT(Area112_ChangeAreaByFlags);
    BOF3_INJECT(Area112_TalkByMember);
    BOF3_INJECT(Area112_ChoiceFlags34);
    BOF3_INJECT(Area112_ChoiceMessage13);
    BOF3_INJECT(Area112_ChoiceClearFlags);
    BOF3_INJECT(Area112_StepHook);
    BOF3_INJECT(Area112_CellHook);
    BOF3_INJECT(Area112_MemberNearBoxes);
    BOF3_INJECT(Area112_EffectRun);
    BOF3_INJECT(Area112_EffectRing);
    BOF3_INJECT(Area113_ChoiceAsk);
    BOF3_INJECT(Area113_Trigger27);
    BOF3_INJECT(Area113_Trigger39);
    BOF3_INJECT(Area113_Trigger63);
    BOF3_INJECT(Area113_TailReward);
    BOF3_INJECT(Area113_Reward);
}

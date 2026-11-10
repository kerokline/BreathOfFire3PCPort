// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_77.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
}

// The choice answer, as every choice of the band reads it: a signed byte.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message word from a choice's table by the signed answer (unchecked: a
// negative answer reads before the table, a large one past it - in .data).
unsigned MessageAt(std::uint32_t table, signed char answer) {
    return Word(Mem(table + static_cast<std::uint32_t>(static_cast<int>(answer) * 2)));
}

// The band's commonest choice: no new message; answer 0 sets counter 0 to
// `on0`, answer 1 to `on1`, any other nothing.
void CountersByAnswer(signed char answer, unsigned char on0, unsigned char on1) {
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = on0;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = on1;
}

void ChoiceCounter0(unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    CountersByAnswer(answer, on0, on1);
}

// The story-flag toggle nobody owns (area_w2a_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

// Area 77's ten spawns (as area 49's SpawnAtMember, docs/area_w1c.md):
// Effect_Spawn(kind, 0, kinds[party list `member`'s first byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The member byte indexes the list unchecked, as the
// original's (a list byte of 8 or more reads the next list).
void SpawnAtMember(unsigned member, unsigned char kind, std::uint32_t kinds) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(kinds + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// A free effect record of kind `kind` (Effect_FindFree; none: nothing), its
// +0 set 1 and +5 the kind; answers the record, or null for none.
unsigned char* SpawnEffectRecord(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    return e;
}

// Camera_ShiftY moved by `by`, MapView_Redraw 2.
void ShiftCamera(int by) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + by);
    MapView_Redraw = 2;
}

}  // namespace

// original 0x40EBB0 (Area_CellHooks' entry for area 0x4D, `(x, z)` answering
// in al): Cond_Flags row 6's flag 0x2C set: 0. The first of
// Area77_CellSwitches' two records whose x and z bytes are the cell's and
// whose pose is the leader's byte +8 (read after the flag test); none: 0.
// Story flag 0x1C set: 0. Else its story flag toggled (0x57C160),
// Sound_PlayEffect(0x201), 0x469FE0(0xF), answer 1.
extern "C" int __cdecl Area77_CellHook(long x, long z) {
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x30), 0x2C)) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    unsigned i = 0;
    for (; i < at::kArea77CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(::bof3::Img(Area77_CellSwitches_addr) + i * 4u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && r[2] == pose) break;
    }
    if (i == at::kArea77CellSwitchCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C)) return 0;
    FlagsToggle(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), B(::bof3::Img(Area77_CellSwitches_addr) + i * 4u + 3));
    AH_CALL(Sound_PlayEffect)(0x201);
    AH_AT(void (__cdecl*)(unsigned), at::kSpawnKind4.v)(0xF);
    return 1;
}

// original 0x40EC50 (Field_ObjectTriggers id 34, (object, flags) ignored):
// ScriptFlags_Set40, then the mode tail kind 0x2C with state 0 and sub-kind
// 7; answers 0 in al.
extern "C" unsigned char __cdecl Area77_Trigger34(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2C;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 7;
    return 0;
}

// original 0x40EC70 (Area77_Choices[0]): the message word from
// Area77_ChoiceMessages by the signed answer, unchecked.
extern "C" void __cdecl Area77_ChoiceMessage(void) { SetMessage(MessageAt(::bof3::Img(Area77_ChoiceMessages_addr), Answer())); }

// original 0x40EC90 (Area77_Choices[1]): no new message; answer 0 sets
// counter 0 to 1, answer 1 to 0.
extern "C" void __cdecl Area77_ChoiceCounter0(void) { ChoiceCounter0(1, 0); }

// original 0x40ECC0 (Area77_Choices[2] = Area77_Handlers[0]; PSX
// 0x801F5A58): Cond_ByteFE 1, Sound_PlayEffect(0x202).
extern "C" void __cdecl Area77_SetByteFE(void) {
    Cond_ByteFE = 1;
    AH_CALL(Sound_PlayEffect)(0x202);
}

// original 0x40ECE0 (Area77_Handlers[1]; PSX 0x801F5A84): Cond_ByteFE 0,
// Sound_PlayEffect(0x202).
extern "C" void __cdecl Area77_ClearByteFE(void) {
    Cond_ByteFE = 0;
    AH_CALL(Sound_PlayEffect)(0x202);
}

// originals 0x40ED00, 0x40ED50, 0x40EDD0, 0x40EE20, 0x40EE70 (handlers 2, 3,
// 6, 7, 8; PSX 0x801F5AAC, 0x801F5B2C, 0x801F5C00, 0x801F5C80, 0x801F5D00):
// SpawnAtMember with lists A..E.
extern "C" void __cdecl Area77_Spawn3AtMember0ListA(void) { SpawnAtMember(0, 3, ::bof3::Img(Area77_EffectKindsA_addr)); }

extern "C" void __cdecl Area77_Spawn4AtMember0ListB(void) { SpawnAtMember(0, 4, ::bof3::Img(Area77_EffectKindsA_addr) + 0x8); }

// original 0x40EDA0 (Area77_Handlers[4]; PSX 0x801F5BAC): Inventory_Remove(1,
// 0x47, 1) - the original pushes a fourth word, 0, that the callee never reads.
extern "C" void __cdecl Area77_RemoveItem47(void) { AH_CALL(Inventory_Remove)(1, 0x47, 1); }

// original 0x40EDC0 (Area77_Handlers[5]; PSX 0x801F5BD8): story flag 4 set.
extern "C" void __cdecl Area77_SetFlag4(void) { AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 4); }

extern "C" void __cdecl Area77_Spawn2AtMember0ListC(void) { SpawnAtMember(0, 2, ::bof3::Img(Area77_EffectKindsA_addr) + 0x10); }

extern "C" void __cdecl Area77_Spawn3AtMember1ListD(void) { SpawnAtMember(1, 3, ::bof3::Img(Area77_EffectKindsA_addr) + 0x18); }

extern "C" void __cdecl Area77_Spawn3AtMember2ListE(void) { SpawnAtMember(2, 3, ::bof3::Img(Area77_EffectKindsA_addr) + 0x20); }

// original 0x40EEC0 (Area77_Handlers[9]; PSX 0x801F5D80): Camera_ShiftY -
// 0x14, MapView_Redraw 2, Sound_PlayEffect(0x200).
extern "C" void __cdecl Area77_ShiftCameraDownSound(void) {
    ShiftCamera(-0x14);
    AH_CALL(Sound_PlayEffect)(0x200);
}

// originals 0x40EEE0, 0x40EF30, 0x40EF80 (handlers 12, 13, 14; PSX
// 0x801F5E04, 0x801F5E84, 0x801F5F04): SpawnAtMember kind 4 for members 1,
// 2, 0, all three by list 0.
extern "C" void __cdecl Area77_Spawn4AtMember1List0(void) { SpawnAtMember(1, 4, ::bof3::Img(Area77_EffectKinds0_addr)); }

extern "C" void __cdecl Area77_Spawn4AtMember2List0(void) { SpawnAtMember(2, 4, ::bof3::Img(Area77_EffectKinds0_addr)); }

extern "C" void __cdecl Area77_Spawn4AtMember0List0(void) { SpawnAtMember(0, 4, ::bof3::Img(Area77_EffectKinds0_addr)); }

// originals 0x40EFD0, 0x40F020 (handlers 15, 16; PSX 0x801F5F84,
// 0x801F6004): SpawnAtMember for member 0 by lists F and G.
extern "C" void __cdecl Area77_Spawn1AtMember0ListF(void) { SpawnAtMember(0, 1, ::bof3::Img(Area77_EffectKindsA_addr) + 0x28); }

extern "C" void __cdecl Area77_Spawn3AtMember0ListG(void) { SpawnAtMember(0, 3, ::bof3::Img(Area77_EffectKindsA_addr) + 0x30); }

// originals 0x40F070, 0x40F080 (handlers 17, 18; PSX 0x801F6084,
// 0x801F60A4): Area77_SpawnEffectKind(0x4A), (0x4C).
extern "C" void __cdecl Area77_SpawnEffect4A(void) { AH_CALL(Area77_SpawnEffectKind)(0x4A); }

extern "C" void __cdecl Area77_SpawnEffect4C(void) { AH_CALL(Area77_SpawnEffectKind)(0x4C); }

// original 0x40F090 (Area77_Handlers[19]; PSX 0x801F60C4): one step of a
// leap by the movement script. The script's operand byte - the byte two past
// the object's position word +0xA in the running area's +0x10 script
// [object +3] - picks a (dx, dz) pair of Area77_Leaps; Scena06_Leap(the
// script object, dx, dz, 0x40, -0x400, 4, 0). Not 0: the script's position
// back 2 (the op runs again next frame). 0: at operand 4 the running
// object's +0x3E is the ground under it (AreaMap_Elevation) and
// Sprite_SetAnimation(0); then the position on 1. The dx and dz dwords carry
// what the original's registers held above the byte (dx: the pair's offset,
// operand * 2; dz: the script table's address), which Scena06_Leap reads as
// s8 only. The script table and the script are indexed unchecked, as the
// movement-script engine indexes them; a running area past Area_Descriptors'
// 200 aborts where the original reads past the table.
extern "C" void __cdecl Area77_LeapStep(void) {
    unsigned char* const object = MoveScript_Object;
    const unsigned area = Game_AreaNumber;
    if (area >= at::kDescriptorCount)
        bof3::Fatal("Area77_LeapStep: Game_AreaNumber %u past Area_Descriptors' %u", area, at::kDescriptorCount);
    const std::uint32_t descriptor = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Area_Descriptors_addr) + area * 4)));
    const unsigned position = rv::As<records::ScriptContext>(object)->position;
    const std::uint32_t scripts = static_cast<std::uint32_t>(Long(Mem(descriptor + 0x10)));
    const std::uint32_t script = static_cast<std::uint32_t>(Long(Mem(scripts + object[3] * 4u)));
    const unsigned char operand = B(script + position + 2);
    const std::uint32_t pair = operand * 2u;
    const std::uint32_t dx = (pair & 0xFFFFFF00u) | B(::bof3::Img(Area77_Leaps_addr) + pair);
    const std::uint32_t dz = (scripts & 0xFFFFFF00u) | B(::bof3::Img(Area77_Leaps_addr) + pair + 1);
    if (AH_CALL(Scena06_Leap)(object, dx, dz, 0x40, 0xFFFFFC00u, 4, 0)) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    if (operand == 4) {
        const unsigned char* const cur = Sprite_Current;
        const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
        const long ground = AH_CALL(AreaMap_Elevation)(tCur->x, tCur->z);
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(ground));
        AH_CALL(Sprite_SetAnimation)(0);
    }
    AddWord(MoveScript_Object + 0xA, 1);
}

// original 0x40F140 (called by handlers 17 and 18): a free effect record of
// kind `kind` (its low byte), +0 = 1. The answer is unread.
extern "C" void __cdecl Area77_SpawnEffectKind(unsigned kind) { SpawnEffectRecord(static_cast<unsigned char>(kind)); }

// original 0x40F170 (Area78_Choices[0]): no new message; answer 0: counter 0
// 0x1E and flag 0x16 of the chapter's row; answer 1: counter 0 0x1E and flag
// 0x17.
extern "C" void __cdecl Area78_ChoiceRowFlag16or17(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0 && answer != 1) return;
    unsigned char* const row = FlagRow();
    B(::bof3::Img(at::kCounter0.v)) = 0x1E;
    AH_CALL(Flags_Set)(row, answer == 0 ? 0x16 : 0x17);
}

// original 0x40F1C0 (Area78_Choices[3], [4]): counter 0 0x2B for answer 0,
// 0x2C for 1.
extern "C" void __cdecl Area78_ChoiceCounter2Bor2C(void) { ChoiceCounter0(0x2B, 0x2C); }

// original 0x40F1F0 (Area78_Choices[5], [6]): counter 0 0xA for answer 0, 2
// for 1.
extern "C" void __cdecl Area78_ChoiceCounterAor2(void) { ChoiceCounter0(0xA, 2); }

void Area77_Inject() {
    BOF3_INJECT(Area77_CellHook);
    BOF3_INJECT(Area77_Trigger34);
    BOF3_INJECT(Area77_ChoiceMessage);
    BOF3_INJECT(Area77_ChoiceCounter0);
    BOF3_INJECT(Area77_SetByteFE);
    BOF3_INJECT(Area77_ClearByteFE);
    BOF3_INJECT(Area77_Spawn3AtMember0ListA);
    BOF3_INJECT(Area77_Spawn4AtMember0ListB);
    BOF3_INJECT(Area77_RemoveItem47);
    BOF3_INJECT(Area77_SetFlag4);
    BOF3_INJECT(Area77_Spawn2AtMember0ListC);
    BOF3_INJECT(Area77_Spawn3AtMember1ListD);
    BOF3_INJECT(Area77_Spawn3AtMember2ListE);
    BOF3_INJECT(Area77_ShiftCameraDownSound);
    BOF3_INJECT(Area77_Spawn4AtMember1List0);
    BOF3_INJECT(Area77_Spawn4AtMember2List0);
    BOF3_INJECT(Area77_Spawn4AtMember0List0);
    BOF3_INJECT(Area77_Spawn1AtMember0ListF);
    BOF3_INJECT(Area77_Spawn3AtMember0ListG);
    BOF3_INJECT(Area77_SpawnEffect4A);
    BOF3_INJECT(Area77_SpawnEffect4C);
    BOF3_INJECT(Area77_LeapStep);
    BOF3_INJECT(Area77_SpawnEffectKind);
    BOF3_INJECT(Area78_ChoiceRowFlag16or17);
    BOF3_INJECT(Area78_ChoiceCounter2Bor2C);
    BOF3_INJECT(Area78_ChoiceCounterAor2);
}

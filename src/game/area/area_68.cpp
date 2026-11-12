// Moved here by tools/move_funcs.py from area_w1f.cpp (docs/source-layout-round.md).
#include "game/area/area_68.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// A record's index among the field objects as the originals compute it: the
// pointer less Sprite_Objects, divided by 0xA4 as a signed dword (imul by
// 0x63E7063F, sar 6, the sign added back): C's truncating division.
unsigned char ObjectIndex(const unsigned char* record) {
    const auto delta = static_cast<std::int32_t>(Low32(record) -
                                                 ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kObjectStride));
}

// A two-entry .data state table read in place by the running object's +4 and
// jumped through (area 68's handlers 2 and 3, 69's 0, 75's 7 and 11). The
// original indexes it unchecked: a state of 2 or more jumps through the bytes
// after the table - ours aborts there.
void RunState(const char* who, std::uint32_t table) {
    const unsigned state = Sprite_Current[4];
    if (state >= 2)
        bof3::Fatal("%s: the running object's state %u is past its 2-entry table 0x%X (the original jumps through the "
                    "bytes after it)",
                    who, state, static_cast<unsigned>(table));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + state * 4)))))();
}

// Area 68's handlers 0, 1, 4, 5, 7, 8 (0x40CFD0 ...): Effect_Spawn(kind, 0,
// kinds[party list `member`'s first byte], party record `member`'s words +0x2E
// and +0x30) with Sprite_Current made that record (and left so); an answer
// other than 0xFF stored to Sprite_Current[0xB], read again after the call.
// The list byte indexes the table unchecked (it stays in .data).
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

// The glides' state 0 (0x40D090, 0x40D380): the running object's count +0xA =
// `count`, its state 1, and the leader's script word +0x12E (through
// Field_State) back by 2.
void GlideBegin(unsigned char count) {
    Sprite_Current[0xA] = count;
    Sprite_Current[4] = 1;
    AddWord(Field_State + 0x12E, 0xFFFE);
}

// The glides' state 1 (0x40D0C0, 0x40D150, 0x40D3B0): with the count +0xA at 0
// the state goes back to 0; else x +0x34 moves by steps[count & 0xF] << 11 and
// z +0x38 by -steps[count & 0xF] << 11 (signed bytes), the count goes down by
// one and the script word +0x12E back by 2.
void GlideStep(std::uint32_t steps) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    const unsigned char count = cur[0xA];
    if (count == 0) {
        cur[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (count & 0xF)))));
    tCur->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->x) + (dx << 11));
    const auto dz = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (cur[0xA] & 0xF)))));
    tCur->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) + ((0u - dz) << 11));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    AddWord(Field_State + 0x12E, 0xFFFE);
}

}  // namespace

// original 0x40CEF0 (Area68_Choices[0]): the message word from
// Area68_ChoiceMessages by the s8 answer (unchecked; it stays in .data);
// answer 0: counter 0 = 0x14; 1: counter 0 = 3.
extern "C" void __cdecl Area68_ChoiceMessageA(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(Area68_ChoiceMessages_addr)) + answer * 2))));
    if (answer == 0)
        B(::bof3::Img(at::kCounter0.v)) = 0x14;
    else if (answer == 1)
        B(::bof3::Img(at::kCounter0.v)) = 3;
}

// original 0x40CF20 (Area68_Choices[1]): the same from the table's second
// pair; answer 0: counter 0 = 0; 1: counter 0 = 0xFF.
extern "C" void __cdecl Area68_ChoiceMessageB(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(Area68_ChoiceMessages_addr) + 4) + answer * 2))));
    if (answer == 0)
        B(::bof3::Img(at::kCounter0.v)) = 0;
    else if (answer == 1)
        B(::bof3::Img(at::kCounter0.v)) = 0xFF;
}

// original 0x40CF50 (Area68_Choices[2]): answer not 0: message 0x84 and the
// mark 6; 0: Inventory_CountUsed(1) at 0xF or more message 0x82, else 0x83 and
// the mark.
//
// DIV-0219 EasierMasters (off by default): the threshold is 0xA, not 0xF -
// the Definitive Edition romhack's change; evidence: its patched US disc,
// AREA068 section 13 at PSX 0x801F38E8 (the compare's immediate).
extern "C" void __cdecl Area68_ChoiceAskCount(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x84);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    const unsigned threshold = bof3::BehaviourOn(bof3::behaviours::Id::EasierMasters) ? 0xA : 0xF;
    if (AH_CALL(Inventory_CountUsed)(1) >= threshold) {
        SetMessage(0x82);
        return;
    }
    SetMessage(0x83);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x40CFA0 (Area68_Choices[5], [6]; Area50_Choices[3], [4]): answer
// not 0: message 0x84 and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area68_ChoiceAnswer84(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x84);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40CFD0 (Area68_Handlers[0]; PSX 0x801F3A18): SpawnAtMember(1, 4,
// Area68_EffectKinds4).
extern "C" void __cdecl Area68_SpawnKind4AtMember1(void) { SpawnAtMember(1, 4, ::bof3::Img(Area68_EffectKinds4_addr)); }

// original 0x40D020 (Area68_Handlers[1]; PSX 0x801F3A98): member 2.
extern "C" void __cdecl Area68_SpawnKind4AtMember2(void) { SpawnAtMember(2, 4, ::bof3::Img(Area68_EffectKinds4_addr)); }

// original 0x40D070 (Area68_Handlers[2]; PSX 0x801F3B18): jmp through
// Area68_GlideStatesA by the running object's +4.
extern "C" void __cdecl Area68_GlideRunA(void) { RunState("Area68_GlideRunA", ::bof3::Img(Area68_GlideStatesA_addr)); }

// original 0x40D090 (Area68_GlideStatesA[0] and Area68_GlideStatesB[0]):
// GlideBegin(0x10).
extern "C" void __cdecl Area68_GlideBegin10(void) { GlideBegin(0x10); }

// original 0x40D0C0 (Area68_GlideStatesA[1]): GlideStep over
// Area68_GlideStepsA.
extern "C" void __cdecl Area68_GlideStepA(void) { GlideStep(::bof3::Img(Area68_GlideStepsA_addr)); }

// original 0x40D130 (Area68_Handlers[3]; PSX 0x801F3C2C): jmp through
// Area68_GlideStatesB.
extern "C" void __cdecl Area68_GlideRunB(void) { RunState("Area68_GlideRunB", ::bof3::Img(Area68_GlideStatesB_addr)); }

// original 0x40D150 (Area68_GlideStatesB[1]): GlideStep over
// Area68_GlideStepsB.
extern "C" void __cdecl Area68_GlideStepB(void) { GlideStep(::bof3::Img(Area68_GlideStepsB_addr)); }

// original 0x40D1C0 / 0x40D210 (Area68_Handlers[4], [5]; PSX 0x801F3D40,
// 0x801F3DC0): kind 1 from Area68_EffectKinds, members 1 and 2.
extern "C" void __cdecl Area68_SpawnKind1AtMember1(void) { SpawnAtMember(1, 1, ::bof3::Img(Area68_EffectKinds_addr)); }

extern "C" void __cdecl Area68_SpawnKind1AtMember2(void) { SpawnAtMember(2, 1, ::bof3::Img(Area68_EffectKinds_addr)); }

// original 0x40D260 / 0x40D2B0 (Area68_Handlers[7], [8]; PSX 0x801F3E48,
// 0x801F3EC8): kind 3.
extern "C" void __cdecl Area68_SpawnKind3AtMember1(void) { SpawnAtMember(1, 3, ::bof3::Img(Area68_EffectKinds_addr)); }

extern "C" void __cdecl Area68_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3, ::bof3::Img(Area68_EffectKinds_addr)); }

// original 0x40D300 (Field_ObjectTriggers id 25; its arguments not read):
// ScriptFlags_Set40; the focus object's +1 = 4, +0x83 = 5, word +0x8A = 0
// (the pointer read again for each store); tail kind 4 with sub-kind 3;
// counter 3 = the focus object's index among the field objects. al 0.
extern "C" unsigned char __cdecl Area68_Trigger25(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    area_harness::Pointer(::bof3::Img(AreaW1a_FocusObject_addr))[1] = 4;
    area_harness::Pointer(::bof3::Img(AreaW1a_FocusObject_addr))[0x83] = 5;
    SetWord(area_harness::Pointer(::bof3::Img(AreaW1a_FocusObject_addr)) + 0x8A, 0);
    const unsigned char index = ObjectIndex(area_harness::Pointer(::bof3::Img(AreaW1a_FocusObject_addr)));
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 3;
    B(::bof3::Img(at::kCounter3.v)) = index;
    return 0;
}

void Area68_Inject() {
    BOF3_INJECT(Area68_ChoiceMessageA);
    BOF3_INJECT(Area68_ChoiceMessageB);
    BOF3_INJECT(Area68_ChoiceAskCount);
    BOF3_INJECT(Area68_ChoiceAnswer84);
    BOF3_INJECT(Area68_SpawnKind4AtMember1);
    BOF3_INJECT(Area68_SpawnKind4AtMember2);
    BOF3_INJECT(Area68_GlideRunA);
    BOF3_INJECT(Area68_GlideBegin10);
    BOF3_INJECT(Area68_GlideStepA);
    BOF3_INJECT(Area68_GlideRunB);
    BOF3_INJECT(Area68_GlideStepB);
    BOF3_INJECT(Area68_SpawnKind1AtMember1);
    BOF3_INJECT(Area68_SpawnKind1AtMember2);
    BOF3_INJECT(Area68_SpawnKind3AtMember1);
    BOF3_INJECT(Area68_SpawnKind3AtMember2);
    BOF3_INJECT(Area68_Trigger25);
}

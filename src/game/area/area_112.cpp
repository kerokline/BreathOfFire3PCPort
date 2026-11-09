// Moved here by tools/move_funcs.py from area_w2f.cpp (docs/source-layout-round.md).
#include "game/area/area_112.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2f::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// Party record m (ObjTrio, 0x14C bytes; m not checked).
unsigned char* Record(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }

void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }

void Clear(unsigned index) { AH_CALL(Flags_Clear)(StoryFlags(), index); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

}  // namespace

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
        const unsigned char key = B(::bof3::Img(Area112_MemberKeys_addr) + k);
        for (unsigned m = 0; m < count; ++m) {
            if (Record(m)[0x89] != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(::bof3::Img(Area112_Messages_addr) + k * 2)));
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
    SetMessage(static_cast<std::uint16_t>(static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))) + 3));
    Set(0x33);
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)))) {
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
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x68), 0x10) != 0) return;
    SetMessage(2);
    Clear(0x33);
}

// original 0x418570 (area 112 +0x34[1] = +0x3C[4]; PSX 0x801F3870): the
// message word 0x14 for an answer, else 0x13.
extern "C" void __cdecl Area112_ChoiceMessage13(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 0x14u : 0x13u); }

// original 0x418590 (area 112 +0x34[2] = +0x3C[5]; PSX 0x801F3898): the
// message word 0xFFFF; answer 0: story flags 0x3C..0x3F and 0x38..0x3B
// cleared (in that order), Sound_PlayEffect(0x200).
extern "C" void __cdecl Area112_ChoiceClearFlags(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
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
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x68), 0x10) == 0) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(x) >> 16) - 0xB) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(z) >> 16) - 0x57) >= 3) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(::bof3::Img(at::kCounter0.v)) = 0;
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
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    unsigned i = 0;
    for (; i < at::kArea112HookCellCount; ++i) {
        const unsigned char* const e = Mem(::bof3::Img(Area112_HookCells_addr) + i * 4);
        if (cx == e[0] && cz == e[1] && pose != 3) break;
    }
    if (i == at::kArea112HookCellCount) return 0;
    if (Test(0x1C)) return 0;
    const unsigned char* const e = Mem(::bof3::Img(Area112_HookCells_addr) + i * 4);
    if (Test(e[3])) return 0;
    if (AH_CALL(Area112_MemberNearBoxes)() != 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xA;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0x15;
        return 0;
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (((e[2] >> bit) & 1) == 0) continue;
        const unsigned char flag = B(::bof3::Img(Area112_HookFlags_addr) + (bit & 3));
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
        const auto* const tR = rv::AsConst<records::PartyMember>(r);
        const U n = r[9];
        const U px = static_cast<U>(tR->step_x) * n + static_cast<U>(tR->x);
        const U pz = static_cast<U>(tR->step_z) * n + static_cast<U>(tR->z);
        const std::int32_t hx = static_cast<std::int32_t>(px) >> 16;
        const std::int32_t hz = static_cast<std::int32_t>(pz) >> 16;
        for (unsigned b = 0; b < at::kArea112BoxCount; ++b) {
            const unsigned char* const box = Mem(::bof3::Img(Area112_Boxes_addr) + b * 4);
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
    StateEntry("Area112_EffectRun", ::bof3::Img(Area112_EffectStates_addr), at::kArea112EffectStateCount, Sprite_Current[1])();
}

// original 0x418880 (Area112_EffectStates[1]): Area100_EffectB7Ring's code -
// the point (x +0x34, z +0x38, y +0x3C) of the running record copied to the
// stack and handed to 0x4220D0 (area_w2f_callees.h).
extern "C" void __cdecl Area112_EffectRing(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    long point[4];
    point[0] = tCur->x;
    point[1] = tCur->z;
    point[2] = tCur->height;
    point[3] = 0;
    AH_AT(void (__cdecl*)(const long*), at::kRingAt.v)(point);
}

// original 0x4188B0 (area 113 +0x34[0]): an answer: the message word 4 and
// the mark 6. Answer 0: the byte 0x9045F4 at 6 or more: the message 2; below:
// the message 3 and the mark 6.
extern "C" void __cdecl Area113_ChoiceAsk(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(4);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    if (B(::bof3::Img(FieldE1_Rank_addr)) >= 6) {
        SetMessage(2);
        return;
    }
    SetMessage(3);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x4188F0 (Field_ObjectTriggers id 27; a gap of the tool):
// ScriptFlags_Set40, tail kind 4 with sub-kind 6 (the state not written); al
// 0.
extern "C" unsigned char __cdecl Area113_Trigger27(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 6;
    return 0;
}

// original 0x418910 (Field_ObjectTriggers id 39; a gap of the tool):
// ScriptFlags_Set40, tail kind 0x2C, state 0, sub-kind 0xD; al 0.
extern "C" unsigned char __cdecl Area113_Trigger39(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2C;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xD;
    return 0;
}

// original 0x418930 (Field_ObjectTriggers id 63; a gap of the tool):
// ScriptFlags_Set40, tail kind 0x3E (Area113_TailReward), state 0, the
// object's word +0x88 = 5; al 0.
extern "C" unsigned char __cdecl Area113_Trigger63(unsigned char* object, unsigned char* flags) {
    (void)flags;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x3E;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->trigger_word, 5);
    return 0;
}

void Area112_Inject() {
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
}

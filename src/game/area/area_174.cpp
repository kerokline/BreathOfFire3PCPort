// Moved here by tools/move_funcs.py from area_w4c.cpp (docs/source-layout-round.md).
#include "game/area/area_174.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

using ObjectFn = void (__cdecl*)(unsigned char*);

using SlotFn = unsigned (__cdecl*)(unsigned char*, const unsigned char*);

unsigned char& B(U address) { return *Mem(address); }

U Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The movement script back (or on) by `delta` words: the running script
// object's u16 +0xA, the pointer read at the store.
void ScriptStep(unsigned delta) { AddWord(MoveScript_Object + 0xA, delta); }

// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). The handlers write
// through Effect_FindFree's slot, or one an earlier handler kept in the
// running object's +0xB, unchecked: past the 20 records the original writes
// into what follows them - ours aborts.
unsigned char* EffectAt(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride));
    return Mem(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
}

// Field_ActiveMember's index among the field objects as the originals compute
// it: (the pointer - Sprite_Objects) / 0xA4, C's truncating signed division
// (imul 0x63E7063F, sar 6, plus the sign bit); only its low byte is kept.
unsigned char ActiveMemberIndex() {
    const auto diff = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(diff / static_cast<std::int32_t>(at::kObjectStride));
}

// Areas 173 and 174's effect of kind 0x9D (handlers 5 and 6 of 173, 3 and 4
// of 174): +0 = 1, kind +5 = 0x9D, +6 = `sub`, +0xB = Field_ActiveMember's
// index byte, word +0x2E = `word2E`, word +0x30 = 0xA5.
void Effect9D(const char* who, unsigned slot, unsigned char sub, unsigned char index, unsigned word2E) {
    unsigned char* const e = EffectAt(who, slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x9D;
    e[6] = sub;
    e[0xB] = index;
    tE->screen_x = word2E;
    tE->screen_y = 0xA5;
}

// Area 174's effects of kinds 0xA2 (handlers 9 and 12): Effect_FindFree into
// the running object's +0xB; none: the script back 2. A slot: +0 = 1, kind +5
// = 0xA2, the object's x, z dwords and its y + 0x1000000, then +1 = `state`.
// The slot is read again from +0xB before each store, as the original does.
void EffectA2(const char* who, unsigned char state) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    if (cur[0xB] == 0xFF) {
        ScriptStep(0xFFFE);
        return;
    }
    EffectAt(who, cur[0xB])[0] = 1;
    EffectAt(who, cur[0xB])[5] = 0xA2;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(who, cur[0xB]))->x, tCur->x);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(who, cur[0xB]))->z, tCur->z);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(who, cur[0xB]))->height, static_cast<std::int32_t>(static_cast<U>(tCur->height) + 0x1000000u));
    EffectAt(who, cur[0xB])[1] = state;
}

// Area 174's effects of kinds 0xA1 and 0xA3 (handlers 10 and 19):
// Effect_FindFree into the running object's +0xB; none: the script back 2. A
// slot: +0 = 1, kind +5, +6 = 1, +7 = 0, +0xB = 1, dword +0x4C = the running
// object.
void EffectOnObject(const char* who, unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) {
        ScriptStep(0xFFFE);
        return;
    }
    EffectAt(who, cur[0xB])[0] = 1;
    EffectAt(who, cur[0xB])[5] = kind;
    EffectAt(who, cur[0xB])[6] = 1;
    EffectAt(who, cur[0xB])[7] = 0;
    EffectAt(who, cur[0xB])[0xB] = 1;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(who, cur[0xB]))->anim_data, static_cast<std::int32_t>(Key(cur)));
}

// The running script's bytes as areas 174 and 198's handlers read them: the
// descriptor of Game_AreaNumber (u16, unchecked), its +0x10 table by the
// script object's +3 (unchecked), at the script object's u16 +0xA.
const unsigned char* ScriptBase() {
    const unsigned char* const desc = Area_Descriptors[Game_AreaNumber];
    const U table = static_cast<U>(Long(desc + at::kDescScripts));
    return reinterpret_cast<const unsigned char*>(
        static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + MoveScript_Object[3] * 4u)))));
}

// Area174_PoseTables[b] (read in place, unchecked).
const unsigned char* PoseTable(unsigned char b) {
    return reinterpret_cast<const unsigned char*>(
        static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(::bof3::Img(Area174_PoseTables_addr) + b * 4u)))));
}

// Area 174's handlers 13 and 14: the running object's direction +8 turned one
// step (`step` 1 or 7, & 7) toward the script's byte +2; there already: the
// script on 2 (the script object read at entry); else Area174_SetPose(the pose
// table of the script's byte +3, the new direction), the script back 2.
void TurnToScript(unsigned char step) {
    unsigned char* const object = MoveScript_Object;
    const unsigned offset = rv::As<records::ScriptContext>(object)->position;
    const unsigned char* const script = ScriptBase();
    unsigned char* const cur = Sprite_Current;
    if (cur[8] == script[offset + 2]) {
        rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, offset + 2);
        return;
    }
    cur[8] = static_cast<unsigned char>((cur[8] + step) & 7);
    const unsigned char direction = Sprite_Current[8];
    const unsigned char b = script[rv::As<records::ScriptContext>(MoveScript_Object)->position + 3];
    AH_CALL(Area174_SetPose)(PoseTable(b), direction);
    ScriptStep(0xFFFE);
}

// Area 174's fade states (0x428FC0 and 0x429080), by the running object's +4
// through a two-entry table on the stack.
constexpr U kFadeStates[2] = {bof3::addr::Area174_FadeTintUp, bof3::addr::Area174_FadeOut};

}  // namespace

// original 0x428870 (Area174_Handlers[0]; PSX 0x801F3C8C): 0x454A80(the
// running object) - its Field_Slots scripts released; 0x455290(the running
// object, read again, Area174_SlotScript) - one started (its slot not read);
// the object (read again) +0x2A = 1; Sprite_SetAnimation(9).
extern "C" void __cdecl Area174_RestartSlotScript(void) {
    AH_AT(ObjectFn, area_w4c::kSlotsReleaseFor.v)(Sprite_Current);
    AH_AT(SlotFn, area_w4c::kSlotStart.v)(Sprite_Current, Mem(::bof3::Img(Area174_SlotScript_addr)));
    Sprite_Current[0x2A] = 1;
    AH_CALL(Sprite_SetAnimation)(9);
}

// original 0x4288B0 (Area174_Handlers[2]; PSX 0x801F3D40): Field_Request 5:
// the script back 2.
extern "C" void __cdecl Area174_WaitWhileRequest5(void) {
    if (Field_Request == 5) ScriptStep(0xFFFE);
}

// original 0x4288D0 (Area174_Handlers[3]; PSX 0x801F3D78): Effect_FindFree; a
// slot: kind 0x9D, +6 = 0, +0xB Field_ActiveMember's index byte n, word +0x2E
// = n * 24 + 0xC, +0x30 = 0xA5. The running object's +0xB = the slot (0xFF
// for none too).
extern "C" void __cdecl Area174_Effect9DSub0(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        const unsigned char n = ActiveMemberIndex();
        Effect9D("Area174_Effect9DSub0", slot, 0, n, n * 24u + 0xC);
    }
    Sprite_Current[0xB] = slot;
}

// original 0x428960 (Area174_Handlers[4]; PSX 0x801F3E68): the same with +6 =
// 1, the running object's +0xB not written.
extern "C" void __cdecl Area174_Effect9DSub1(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    const unsigned char n = ActiveMemberIndex();
    Effect9D("Area174_Effect9DSub1", slot, 1, n, n * 24u + 0xC);
}

// original 0x4289E0 (Area174_Handlers[5]; PSX 0x801F3F4C): the effect in the
// running object's +0xB: +1 = 3, +0x5D = 0x70, +0x5E = 0x30 (the slot read
// again for each).
extern "C" void __cdecl Area174_EffectState3(void) {
    unsigned char* const cur = Sprite_Current;
    EffectAt("Area174_EffectState3", cur[0xB])[1] = 3;
    EffectAt("Area174_EffectState3", cur[0xB])[0x5D] = 0x70;
    EffectAt("Area174_EffectState3", cur[0xB])[0x5E] = 0x30;
}

// original 0x428A20 (Area174_Handlers[6]; PSX 0x801F3FFC): the running
// object's word +0x3E less 0x14; each of its bytes +0x5D, +0x5E, +0x5F below
// -0x40 (signed) raised by 2; the word at 0xA00: +0 bit 5 cleared, +0x5C,
// +0x5F, +0x5E, +0x5D = 0. Else Field_ActiveMember's word +0x8A less 2.
extern "C" void __cdecl Area174_SinkAndBrighten(void) {
    AddWord(Sprite_Current + 0x3E, 0xFFEC);
    unsigned char* const cur = Sprite_Current;
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const unsigned char c = cur[k];
        if (static_cast<signed char>(c) < -0x40) cur[k] = static_cast<unsigned char>(c + 2);
    }
    if (rv::As<records::SpriteObject>(cur)->height_whole == 0xA00) {
        cur[0] = static_cast<unsigned char>(cur[0] & 0xDF);
        Sprite_Current[0x5C] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5D] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, 0xFFFE);
}

// original 0x428AB0 (Area174_Handlers[7]; PSX 0x801F4100; also area 198's
// handler 1 - one body, in area 174's block):
// Sprite_SetAnimationAt(the script's byte +2, the running object's word +0x58
// less 2); the object (read again) +0x2A = the script's byte +3; the script
// (its object read again) on 2.
extern "C" void __cdecl Area174_ScriptAnimationAt(void) {
    const unsigned char* const script = ScriptBase();
    const unsigned offset = rv::As<records::ScriptContext>(MoveScript_Object)->position;
    const unsigned char pose = script[offset + 3];
    const unsigned char animation = script[offset + 2];
    const auto start = static_cast<unsigned short>(rv::Current<records::SpriteObject>()->script_pos - 2u);
    AH_CALL(Sprite_SetAnimationAt)(animation, start);
    Sprite_Current[0x2A] = pose;
    ScriptStep(2);
}

// original 0x428B10 (Area174_Handlers[8]; PSX 0x801F41A8; also area 198's
// handler 2 - one body, in area 174's block): the running object's word +0x58
// at 2: Sprite_SetAnimationAt(the script's byte +2, 0), +0x2A = the script's
// byte +3, the script on 2; else the script back 2.
extern "C" void __cdecl Area174_ScriptAnimationOn2(void) {
    if (rv::Current<records::SpriteObject>()->script_pos != 2) {
        ScriptStep(0xFFFE);
        return;
    }
    const unsigned char* const script = ScriptBase();
    const unsigned offset = rv::As<records::ScriptContext>(MoveScript_Object)->position;
    const unsigned char pose = script[offset + 3];
    AH_CALL(Sprite_SetAnimationAt)(script[offset + 2], 0);
    Sprite_Current[0x2A] = pose;
    ScriptStep(2);
}

// original 0x428B80 (Area174_Handlers[9]; PSX 0x801F4278): an effect of kind
// 0xA2 at the running object (y + 0x1000000), +1 = 0 (EffectA2 above).
extern "C" void __cdecl Area174_EffectA2State0(void) { EffectA2("Area174_EffectA2State0", 0); }

// original 0x428C10 (Area174_Handlers[10]; PSX 0x801F43FC): an effect of kind
// 0xA1 on the running object (EffectOnObject above).
extern "C" void __cdecl Area174_EffectA1(void) { EffectOnObject("Area174_EffectA1", 0xA1); }

// original 0x428CA0 (Area174_Handlers[11]; PSX 0x801F4594): b = byte +2 of
// Field_State's script (dword +0x130) at the script object's u16 +0xA. The
// byte 0x903848 below b (unsigned): the running object stepped by
// Area174_StepDeltas[+0xA & 0xF] (x += d << 11, z -= d << 11, the index read
// again), +0xA less 1, Field_State's word +0x12E less 2. Else x and z masked
// to 0xFFFF8000, the script on 1.
extern "C" void __cdecl Area174_StepByScript(void) {
    const unsigned offset = rv::As<records::ScriptContext>(MoveScript_Object)->position;
    const unsigned char* const script =
        reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::MemberFieldTail>(Field_State + 0x124)->script))));
    const unsigned char limit = B(::bof3::Img(at::kStepLimit.v));
    const unsigned char b = script[offset + 2];
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    if (limit < b) {
        const auto dx = static_cast<signed char>(B(::bof3::Img(Area174_StepDeltas_addr) + (cur[0xA] & 0xF)));
        tCur->x = static_cast<std::int32_t>(static_cast<U>(tCur->x) + (static_cast<U>(dx) << 11));
        const auto dz = static_cast<signed char>(B(::bof3::Img(Area174_StepDeltas_addr) + (cur[0xA] & 0xF)));
        tCur->z = static_cast<std::int32_t>(static_cast<U>(tCur->z) + ((0u - static_cast<U>(dz)) << 11));
        cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
        AddWord(Field_State + 0x12E, 0xFFFE);
        return;
    }
    tCur->x = static_cast<std::int32_t>(static_cast<U>(tCur->x) & 0xFFFF8000u);
    tCur->z = static_cast<std::int32_t>(static_cast<U>(tCur->z) & 0xFFFF8000u);
    ScriptStep(1);
}

// original 0x428D50 (Area174_Handlers[12]; PSX 0x801F4688): an effect of kind
// 0xA2 at the running object, +1 = 2.
extern "C" void __cdecl Area174_EffectA2State2(void) { EffectA2("Area174_EffectA2State2", 2); }

// original 0x428DE0 (Area174_Handlers[13]; PSX 0x801F4810): the running
// object turned one step clockwise (+1 & 7) toward the script's byte +2
// (TurnToScript above).
extern "C" void __cdecl Area174_TurnRightToScript(void) { TurnToScript(1); }

// original 0x428E70 (Area174_Handlers[14]; PSX 0x801F48EC): the same one step
// the other way (-1 & 7).
extern "C" void __cdecl Area174_TurnLeftToScript(void) { TurnToScript(7); }

// original 0x428F00 (Area174_Handlers[15]; PSX 0x801F49C8):
// Area174_SetPose(the pose table of the script's byte +2, the leader's
// direction ^ 4); the script (its object read again) on 1.
extern "C" void __cdecl Area174_FaceAwayFromLeader(void) {
    const auto direction = static_cast<unsigned char>(B(::bof3::Img(ObjTrio_addr) + 0x8) ^ 4);
    const unsigned char* const script = ScriptBase();
    const unsigned char b = script[rv::As<records::ScriptContext>(MoveScript_Object)->position + 2];
    AH_CALL(Area174_SetPose)(PoseTable(b), direction);
    ScriptStep(1);
}

// original 0x428F50 (called by handlers 13..15): (poses, direction). The
// running object's +8 = the direction; i = (direction << 1) & 0xFF;
// Sprite_SetAnimation(poses[i]); the object (read again) +0x2A = poses[i + 1]
// (read after the call).
extern "C" void __cdecl Area174_SetPose(const unsigned char* poses, unsigned direction) {
    const auto d = static_cast<unsigned char>(direction);
    Sprite_Current[8] = d;
    const unsigned char* const pose = poses + static_cast<unsigned char>(d << 1);
    AH_CALL(Sprite_SetAnimation)(pose[0]);
    Sprite_Current[0x2A] = pose[1];
}

// original 0x428F90 (Area174_Handlers[16]; PSX 0x801F4AB4): the running
// object's state +4 through a two-entry table on the stack (0x428FC0,
// 0x429080), unchecked: past it the original calls through its own frame -
// ours aborts.
extern "C" void __cdecl Area174_FadeRun(void) {
    const unsigned state = Sprite_Current[4];
    if (state >= 2)
        bof3::Fatal("Area174_FadeRun: the running object's state %u is past the 2 entries of its stack table (the "
                    "original calls through the dword after them on its stack)",
                    state);
    area_harness::Phase(kFadeStates[state])();
}

// original 0x428FC0 (Area174_FadeRun's state 0): the running object's word
// +0x3E += 8; Field_ActiveMember's tint record (MoveScript_TintRecords, by its
// +0x9F): byte +2 below 0x1E (signed): +2, +3, +4 += 2 (the index read again),
// the script back 2. Else the object's +0 |= 0x20, +0x5C = 1, +0x5F, +0x5E,
// +0x5D = 0xC0, state 1, the script back 2.
extern "C" void __cdecl Area174_FadeTintUp(void) {
    AddWord(Sprite_Current + 0x3E, 8);
    unsigned char* const member = Field_ActiveMember;
    unsigned char* const tint = Mem(::bof3::Img(MoveScript_TintRecords_addr) + member[0x9F] * at::kTintStride);
    const unsigned char red = tint[2];
    if (static_cast<signed char>(red) < 0x1E) {
        tint[2] = static_cast<unsigned char>(red + 2);
        unsigned char* const again = Mem(::bof3::Img(MoveScript_TintRecords_addr) + member[0x9F] * at::kTintStride);
        again[3] = static_cast<unsigned char>(again[3] + 2);
        unsigned char* const third = Mem(::bof3::Img(MoveScript_TintRecords_addr) + member[0x9F] * at::kTintStride);
        third[4] = static_cast<unsigned char>(third[4] + 2);
        ScriptStep(0xFFFE);
        return;
    }
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x5F] = 0xC0;
    Sprite_Current[0x5E] = 0xC0;
    Sprite_Current[0x5D] = 0xC0;
    Sprite_Current[4] = 1;
    ScriptStep(0xFFFE);
}

// original 0x429080 (Area174_FadeRun's state 1): the running object's word
// +0x3E += 8; its +0x5D at 0x80: +0 |= 0x40, state 0. Else +0x5D less 4, +0x5E
// and +0x5F less 4, the script back 2.
extern "C" void __cdecl Area174_FadeOut(void) {
    AddWord(Sprite_Current + 0x3E, 8);
    unsigned char* const cur = Sprite_Current;
    const unsigned char c = cur[0x5D];
    if (c == 0x80) {
        cur[0] = static_cast<unsigned char>(cur[0] | 0x40);
        Sprite_Current[4] = 0;
        return;
    }
    cur[0x5D] = static_cast<unsigned char>(c - 4);
    Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0x5E] + 0xFC);
    Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0x5F] + 0xFC);
    ScriptStep(0xFFFE);
}

// original 0x4290E0 (Area174_Handlers[17]; PSX 0x801F4D1C): Field_Request 5:
// 0x454A80(the running object) - its Field_Slots scripts released.
extern "C" void __cdecl Area174_ReleaseOnRequest5(void) {
    if (Field_Request == 5) AH_AT(ObjectFn, area_w4c::kSlotsReleaseFor.v)(Sprite_Current);
}

// original 0x429100 (Area174_Handlers[18]; PSX 0x801F4D54):
// Sprite_LoadPalette(0x80D380 + the running object's +5 * 0x40, 1).
extern "C" void __cdecl Area174_LoadPalette(void) {
    AH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + Sprite_Current[5] * 0x40u)), 1);
}

// original 0x429120 (Area174_Handlers[19]; PSX 0x801F4D90): an effect of kind
// 0xA3 on the running object (EffectOnObject above).
extern "C" void __cdecl Area174_EffectA3(void) { EffectOnObject("Area174_EffectA3", 0xA3); }

// original 0x4291B0 (Area174_Handlers[20]; PSX 0x801F4F28): File_LoadDone 0
// (all of eax): the script back 2.
extern "C" void __cdecl Area174_WaitLoad(void) {
    if (AH_CALL(File_LoadDone)() == 0) ScriptStep(0xFFFE);
}

// original 0x4291D0 (Area174_Choices[0] and Area174_Handlers[21]; PSX
// 0x801F4F6C): message 0xFFFF; the byte 0x8034E5 = 0x1E for an answer not 0,
// else 0xA.
extern "C" void __cdecl Area174_ChoiceByteE5(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = answer != 0 ? 0x1E : 0xA;
}

// original 0x4291F0 (choice 4 of areas 175..185): message 0x62 for answer 0,
// else 0x63.
extern "C" void __cdecl Area175_ChoiceMessage62(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 0x63 : 0x62); }

// original 0x429210 (choices 7 and 8 of areas 175..185): message 0xFFFF for
// answer 0, else 0x70.
extern "C" void __cdecl Area175_ChoiceMessage70(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 0x70 : 0xFFFF); }

// original 0x429230 (choices 11 and 12): 0xFFFF for answer 0, else 0x7E.
extern "C" void __cdecl Area175_ChoiceMessage7E(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 0x7E : 0xFFFF); }

// original 0x429250 (choices 16 and 17): 0xFFFF for answer 0, else 0x8A.
extern "C" void __cdecl Area175_ChoiceMessage8A(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 0x8A : 0xFFFF); }

// original 0x429270 (choice 23): message 0xFFFF; the byte 0x939A3C = the
// answer.
extern "C" void __cdecl Area175_ChoiceStore3C(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    B(::bof3::Img(Rest4b_BoardFlag_addr)) = answer;
}

// original 0x429290 (choice 25): answer 0: message 0xFFFF, the byte 0x939A3E =
// 0; else message 0xF7, the byte 4.
extern "C" void __cdecl Area175_ChoiceByte3E(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) {
        SetMessage(0xFFFF);
        B(::bof3::Img(Rest4b_BoardState_addr)) = 0;
        return;
    }
    SetMessage(0xF7);
    B(::bof3::Img(Rest4b_BoardState_addr)) = 4;
}

void Area174_Inject() {
    BOF3_INJECT(Area174_RestartSlotScript);
    BOF3_INJECT(Area174_WaitWhileRequest5);
    BOF3_INJECT(Area174_Effect9DSub0);
    BOF3_INJECT(Area174_Effect9DSub1);
    BOF3_INJECT(Area174_EffectState3);
    BOF3_INJECT(Area174_SinkAndBrighten);
    BOF3_INJECT(Area174_ScriptAnimationAt);
    BOF3_INJECT(Area174_ScriptAnimationOn2);
    BOF3_INJECT(Area174_EffectA2State0);
    BOF3_INJECT(Area174_EffectA1);
    BOF3_INJECT(Area174_StepByScript);
    BOF3_INJECT(Area174_EffectA2State2);
    BOF3_INJECT(Area174_TurnRightToScript);
    BOF3_INJECT(Area174_TurnLeftToScript);
    BOF3_INJECT(Area174_FaceAwayFromLeader);
    BOF3_INJECT(Area174_SetPose);
    BOF3_INJECT(Area174_FadeRun);
    BOF3_INJECT(Area174_FadeTintUp);
    BOF3_INJECT(Area174_FadeOut);
    BOF3_INJECT(Area174_ReleaseOnRequest5);
    BOF3_INJECT(Area174_LoadPalette);
    BOF3_INJECT(Area174_EffectA3);
    BOF3_INJECT(Area174_WaitLoad);
    BOF3_INJECT(Area174_ChoiceByteE5);
    BOF3_INJECT(Area175_ChoiceMessage62);
    BOF3_INJECT(Area175_ChoiceMessage70);
    BOF3_INJECT(Area175_ChoiceMessage7E);
    BOF3_INJECT(Area175_ChoiceMessage8A);
    BOF3_INJECT(Area175_ChoiceStore3C);
    BOF3_INJECT(Area175_ChoiceByte3E);
}

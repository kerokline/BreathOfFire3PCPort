// Moved here by tools/move_funcs.py from area_w2c.cpp (docs/source-layout-round.md).
#include "game/area/area_92.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

// A message word of a table indexed by the signed answer (unchecked).
unsigned MessageAt(U table, int answer) { return Word(Mem(table + static_cast<U>(answer) * 2u)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The scene start the choices share: ScriptFlags_Set40, then the movement
// script's four counters 0, the run step byte after MoveScript_Var7 and
// MoveScript_Var7 itself.
void RunStep(unsigned char var7, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kCounter0.v)) = 0;
    B(::bof3::Img(at::kCounter1.v)) = 0;
    B(::bof3::Img(at::kCounter2.v)) = 0;
    B(::bof3::Img(at::kCounter3.v)) = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
    B(::bof3::Img(MoveScript_Var7_addr)) = var7;
}

// Area 92's handlers 0..6: Effect_Spawn's body written out (the kind, 0, a
// signed byte of `args` by the party list byte at `list`, the record's words
// +0x2E / +0x30), for party record `record`, which Sprite_Current is made (and
// left). Unlike Effect_Spawn's callers, the slot goes to the record's +0xB
// before the test for none (0xFF). Every store reads +0xB again.
void SpawnInline(U record, unsigned char kind, U args, U list) {
    Sprite_Current = Mem(record);
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const r = Sprite_Current;
    auto* const tR = rv::As<records::SpriteObject>(r);
    if (r[0xB] == 0xFF) return;
    EffectAt(r[0xB])[0] = 1;
    EffectAt(r[0xB])[5] = 6;
    EffectAt(r[0xB])[6] = kind;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(r[0xB]))->step_x, 0);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(r[0xB]))->step_z, static_cast<signed char>(B(args + B(list))));
    rv::Store<unsigned>(rv::As<records::EffectObject>(EffectAt(r[0xB]))->screen_x, static_cast<std::uint16_t>(tR->screen_x));
    rv::Store<unsigned>(rv::As<records::EffectObject>(EffectAt(r[0xB]))->screen_y, static_cast<std::uint16_t>(tR->screen_y));
}

}  // namespace

// original 0x4128A0 (area 92 +0x34[0]): the message word of
// Area92_ChoiceMessages by the s8 answer; answer 2, 3, 4: the scene start with
// MoveScript_Var7 8 and step 0xA, 0x1E, 0x14.
extern "C" void __cdecl Area92_ChoiceRunStep(void) {
    const int answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area92_ChoiceMessages_addr), answer));
    switch (answer) {
    case 2: RunStep(8, 0xA); break;
    case 3: RunStep(8, 0x1E); break;
    case 4: RunStep(8, 0x14); break;
    default: break;
    }
}

// original 0x412940 (area 92 +0x34[1]): the message word by the s8 answer;
// answer (byte) 0: the scene start with MoveScript_Var7 8 and step 0.
extern "C" void __cdecl Area92_ChoiceRun0(void) {
    SetMessage(MessageAt(::bof3::Img(Area92_ChoiceMessages_addr) + 0xc, Answer()));
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) RunStep(8, 0);
}

// original 0x412990 (area 92 +0x34[2]): the same with step 0x28.
extern "C" void __cdecl Area92_ChoiceRun28(void) {
    SetMessage(MessageAt(::bof3::Img(Area92_ChoiceMessages_addr) + 0x10, Answer()));
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) RunStep(8, 0x28);
}

// original 0x4129E0 (area 92 +0x34[3]): the message word by the s8 answer;
// answer 0: the scene start with MoveScript_Var7 7, step 0x3C; 1: the
// chapter row's flag 0x1B clear - step 0x12, set - step 0x23.
extern "C" void __cdecl Area92_ChoiceRunByFlag1B(void) {
    const int answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area92_ChoiceMessages_addr) + 0x14, answer));
    if (answer == 0) {
        RunStep(7, 0x3C);
    } else if (answer == 1) {
        if (AH_CALL(Flags_Test)(FlagRow(), 0x1B) == 0)
            RunStep(7, 0x12);
        else
            RunStep(7, 0x23);
    }
}

// original 0x412AA0 (area 92 +0x3C[0] = +0x34[4], PSX 0x801F2E9C): an effect
// of kind 4 at party record 0, its third byte Area92_EffectArgs4 by the first
// party list byte.
extern "C" void __cdecl Area92_SpawnKind4AtMember0(void) { SpawnInline(::bof3::Img(ObjTrio_addr), 4, ::bof3::Img(Area92_EffectArgs4_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x412B50 (+0x3C[1], PSX 0x801F304C): kind 3 at record 0 by list 0,
// Area92_EffectArgs3.
extern "C" void __cdecl Area92_SpawnKind3AtMember0(void) { SpawnInline(::bof3::Img(ObjTrio_addr), 3, ::bof3::Img(Area92_EffectArgs3_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x412C00 (+0x3C[2], PSX 0x801F31FC): kind 3 at record 1 by list 1.
extern "C" void __cdecl Area92_SpawnKind3AtMember1(void) {
    SpawnInline(::bof3::Img(ObjTrio_addr) + at::kPartyStride, 3, ::bof3::Img(Area92_EffectArgs3_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3);
}

// original 0x412CB0 (+0x3C[3], PSX 0x801F33AC): kind 3 at record 2 by list 2
// (read as a dword, masked to its byte).
extern "C" void __cdecl Area92_SpawnKind3AtMember2(void) {
    SpawnInline(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, 3, ::bof3::Img(Area92_EffectArgs3_addr), ::bof3::Img(Cond_Flags_addr) + 0xd4);
}

// original 0x412D70 (+0x3C[4], PSX 0x801F355C): kind 1 at record 0 by list 0,
// Area92_EffectArgs1.
extern "C" void __cdecl Area92_SpawnKind1AtMember0(void) { SpawnInline(::bof3::Img(ObjTrio_addr), 1, ::bof3::Img(Area92_EffectArgs1_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x412E20 (+0x3C[5], PSX 0x801F3708): kind 1 at record 1 by list 1.
extern "C" void __cdecl Area92_SpawnKind1AtMember1(void) {
    SpawnInline(::bof3::Img(ObjTrio_addr) + at::kPartyStride, 1, ::bof3::Img(Area92_EffectArgs1_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3);
}

// original 0x412ED0 (+0x3C[6], PSX 0x801F38B4): kind 1 at record 2 by list 2.
extern "C" void __cdecl Area92_SpawnKind1AtMember2(void) {
    SpawnInline(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, 1, ::bof3::Img(Area92_EffectArgs1_addr), ::bof3::Img(Cond_Flags_addr) + 0xd4);
}

// original 0x412F90 (+0x3C[7] = +0x34[11], PSX 0x801F3A60): Music_Play(0x3D, 8).
extern "C" void __cdecl Area92_PlayMusic3D(void) { AH_CALL(Music_Play)(0x3D, 8); }

// original 0x412FA0 (Field_ObjectTriggers id 37): ScriptFlags_Set40, tail kind
// 0x2C with state 0 and sub-kind 0xB; al 0.
extern "C" unsigned char __cdecl Area92_Trigger37(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2C;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xB;
    return 0;
}

void Area92_Inject() {
    BOF3_INJECT(Area92_ChoiceRunStep);
    BOF3_INJECT(Area92_ChoiceRun0);
    BOF3_INJECT(Area92_ChoiceRun28);
    BOF3_INJECT(Area92_ChoiceRunByFlag1B);
    BOF3_INJECT(Area92_SpawnKind4AtMember0);
    BOF3_INJECT(Area92_SpawnKind3AtMember0);
    BOF3_INJECT(Area92_SpawnKind3AtMember1);
    BOF3_INJECT(Area92_SpawnKind3AtMember2);
    BOF3_INJECT(Area92_SpawnKind1AtMember0);
    BOF3_INJECT(Area92_SpawnKind1AtMember1);
    BOF3_INJECT(Area92_SpawnKind1AtMember2);
    BOF3_INJECT(Area92_PlayMusic3D);
    BOF3_INJECT(Area92_Trigger37);
}

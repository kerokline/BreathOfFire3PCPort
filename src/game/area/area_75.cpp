// Moved here by tools/move_funcs.py from area_w1f.cpp, area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_75.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area/area_75_callees.h"
#include "game/area_harness.h"
#include "game/area_w1f.h"
#include "game/area_w1f_callees.h"
#include "game/area_w2a_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_75::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return rv::Bytes(Sprite_Objects) + index * at::kObjectStride; }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// A record's index among the field objects as the originals compute it: the
// pointer less Sprite_Objects, divided by 0xA4 as a signed dword (imul by
// 0x63E7063F, sar 6, the sign added back): C's truncating division.
unsigned char ObjectIndex(const unsigned char* record) {
    const auto delta = static_cast<std::int32_t>(Low32(record) -
                                                 ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kObjectStride));
}

// A float stored as the originals' fst does.
void SetFloat(unsigned char* p, float v) { std::memcpy(p + 0, &v, sizeof v); }

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

// The press scenes' opening of a move (the tail's states 5 and 0xA): the
// phase 2, the move 0, the rhythm row 0; object A made Sprite_Current, its
// animation 2 ensured, its +0x2A = 1.
void OpenMoveA() {
    const unsigned index = B(::bof3::Img(at::kObjectA.v));
    B(::bof3::Img(AreaW1f_Phase_addr)) = 2;
    B(::bof3::Img(AreaW1f_Move_addr)) = 0;
    B(::bof3::Img(AreaW1f_Row_addr)) = 0;
    Sprite_Current = ObjectAt(index);
    AH_CALL(Sprite_EnsureAnimation)(2);
    Sprite_Current[0x2A] = 1;
}

// Rand() % 2 as the originals take it (and 0x80000001, the sign put back): -1,
// 0 or 1.
int RandHalf() { return AH_CALL(Rand)() % 2; }

// A counter word less (4 + the draw): 0xFFFC - r added as a word.
void DropCounter(std::uint32_t counter, int r) {
    AddWord(Mem(counter), static_cast<unsigned>(0xFFFCu - static_cast<unsigned>(r)));
}

// An effect record's dword +0xC less one.
void DrainEffect(unsigned slot) {
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectWork>(e);   // +0xC a meter the area counts down (C2 audit ea doubt 5)
    tE->work_0c = tE->work_0c - 1;
}

// |player counter - other counter|, the words as the originals subtract them.
int CounterGap() {
    const int d = static_cast<int>(Word(Mem(::bof3::Img(AreaW1f_PlayerCounter_addr)))) - static_cast<int>(Word(Mem(::bof3::Img(AreaW1f_OtherCounter_addr))));
    return d < 0 ? -d : d;
}

// A byte of Input_Pressed's low byte (the originals test the byte 0x7E1BEC).
bool Pressed20() { return (B(::bof3::Img(Input_Pressed_addr)) & Sigma(0x20)) != 0; }

}  // namespace

// original 0x40D7D0 (choice 3 = handler 1): ScriptFlags_Set40, tail kind 30
// at state 0.
extern "C" void __cdecl Area75_ArmTail30(void) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x1E;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x40D7F0 (choice 4 = handler 2): object B's index = the active
// member's among the field objects.
extern "C" void __cdecl Area75_MarkObjectB(void) { B(::bof3::Img(AreaW1f_ObjectB_addr)) = ObjectIndex(Field_ActiveMember); }

// original 0x40D820 (choice 5 = handler 3): object A's index, the same way.
extern "C" void __cdecl Area75_MarkObjectA(void) { B(::bof3::Img(at::kObjectA.v)) = ObjectIndex(Field_ActiveMember); }

// original 0x40D850 (choice 6 = handler 4): the two effect records' +1 = 2
// (the other's first).
extern "C" void __cdecl Area75_ShowEffects(void) {
    const unsigned other = B(::bof3::Img(at::kOtherEffect.v));
    const unsigned player = B(::bof3::Img(AreaW1f_PlayerEffect_addr));
    EffectAt(other)[1] = 2;
    EffectAt(player)[1] = 2;
}

// original 0x40D880 (choice 7 = handler 5): field object 5's +0x83 = 7, word
// +0x8A = 0, +0x84 = 2, +1 = 4.
extern "C" void __cdecl Area75_PoseObject5(void) {
    unsigned char* const object = ObjectAt(5);
    auto* const tObject = rv::As<records::SpriteObject>(object);
    object[0x83] = 7;
    tObject->script_position = 0;
    object[0x84] = 2;
    object[1] = 4;
}

// original 0x40D8A0 (choice 8 = handler 6): five times, Sprite_FindFree into
// the scratch word 0x903850; a slot: EventOp_0x(Area75_SpawnOps record i), and
// the new Sprite_Current put at the running object's x + 0x30000, its z and its
// dword +0x3C (each read after the op), its +0xB = i. Then Sprite_Current and
// Field_ActiveMember put back as they were on entry.
extern "C" void __cdecl Area75_SpawnFive(void) {
    unsigned char* const member = Field_ActiveMember;
    unsigned char* const self = Sprite_Current;
    auto* const tSelf = rv::As<records::SpriteObject>(self);
    std::uint32_t op = ::bof3::Img(Area75_SpawnOps_addr);
    for (unsigned char i = 0; i < 5; ++i, op += at::kArea75SpawnOpStride) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(::bof3::Img(at::kScratch.v)), slot);
        if (slot == 0xFF) continue;
        AH_CALL(EventOp_0x)(Mem(op));
        const std::int32_t x = tSelf->x;
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(x) + 0x30000u));
        const std::int32_t z = tSelf->z;
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, z);
        const std::int32_t y = tSelf->height;
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, y);
        Sprite_Current[0xB] = i;
    }
    Sprite_Current = self;
    Field_ActiveMember = member;
}

// original 0x40D920 (choice 9 = handler 7): jmp through Area75_FlyStates by
// the running object's +4.
extern "C" void __cdecl Area75_FlyRun(void) { RunState("Area75_FlyRun", ::bof3::Img(Area75_FlyStates_addr)); }

// original 0x40D940 (Area75_FlyStates[0]): the running object's dword +0x14 =
// Area75_FlyHeights[its +0xB] (unchecked; it stays in .data), +0x2B = 3, state
// 1; the script object's position word +0xA back by 2.
extern "C" void __cdecl Area75_FlyBegin(void) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    tCur->step_y = B(::bof3::Img(Area75_FlyHeights_addr) + cur[0xB]);
    Sprite_Current[0x2B] = 3;
    Sprite_Current[4] = 1;
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40D980 (Area75_FlyStates[1]): the running object's +0x5D at 0x80:
// its +0 = 0 (gone). Else +0x5D less 8, +0x5E and +0x5F plus 0xF8 (bytes);
// x +0x34 plus +0xC, z +0x38 plus +0x10, word +0x3E plus word +0x14; the
// script position back by 2.
extern "C" void __cdecl Area75_FlyStep(void) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    if (cur[0x5D] == 0x80) {
        cur[0] = 0;
        return;
    }
    cur[0x5D] = static_cast<unsigned char>(cur[0x5D] - 8);
    cur[0x5E] = static_cast<unsigned char>(cur[0x5E] + 0xF8);
    cur[0x5F] = static_cast<unsigned char>(cur[0x5F] + 0xF8);
    tCur->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->x) + static_cast<std::uint32_t>(tCur->step_x));
    tCur->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) + static_cast<std::uint32_t>(tCur->step_z));
    AddWord(cur + 0x3E, Word(cur + 0x14));
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40D9F0 (choice 12 = handler 10): one frame in four the two effect
// records' dwords +0xC less 1 (the other's first); counter 0 below 0x20: the
// script position back by 2.
extern "C" void __cdecl Area75_DrainEffects(void) {
    const unsigned bit = (Frame_Counter & 3) == 0 ? 1u : 0u;
    unsigned char* e = EffectAt(B(::bof3::Img(at::kOtherEffect.v)));
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(e)->work_0c, static_cast<std::int32_t>(rv::As<records::EffectWork>(e)->work_0c - bit));
    e = EffectAt(B(::bof3::Img(AreaW1f_PlayerEffect_addr)));
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(e)->work_0c, static_cast<std::int32_t>(rv::As<records::EffectWork>(e)->work_0c - bit));
    if (B(::bof3::Img(at::kCounter0.v)) < 0x20) AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40DA60 (choice 13 = handler 11): jmp through Area75_FollowStates
// by the running object's +4.
extern "C" void __cdecl Area75_FollowRun(void) { RunState("Area75_FollowRun", ::bof3::Img(Area75_FollowStates_addr)); }

// original 0x40DA80 (Area75_FollowStates[0]): Sound_PlayById(0x200), the
// script position back by 2, the running object's state 1 (both read after
// the call).
extern "C" void __cdecl Area75_FollowBegin(void) {
    AH_CALL(Sound_PlayById)(0x200);
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
    Sprite_Current[4] = 1;
}

// original 0x40DAB0 (Area75_FollowStates[1]): Field_Request not 0: the running
// object's z = Field_Kind2Z and the script position back by 2; 0: its state 0.
extern "C" void __cdecl Area75_FollowStep(void) {
    if (Field_Request != 0) {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Field_Kind2Z);
        AddWord(MoveScript_Object + 0xA, 0xFFFE);
        return;
    }
    Sprite_Current[4] = 0;
}

// original 0x40DAE0 (choice 14 = handler 12): Field_Request 5:
// Sound_PlayEffect(0x207); else the running object's z = Field_Kind2Z and the
// script position back by 2.
extern "C" void __cdecl Area75_FollowKind2Z(void) {
    if (Field_Request == 5) {
        AH_CALL(Sound_PlayEffect)(0x207);
        return;
    }
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Field_Kind2Z);
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40DB20 (Area75_Choices[0]): answer 0: message 0x19, the tail
// state 0x14, counter 0 = 0x15; else message 0x1A.
extern "C" void __cdecl Area75_ChoiceStart14(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x1A);
        return;
    }
    SetMessage(0x19);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
    B(::bof3::Img(at::kCounter0.v)) = 0x15;
}

// original 0x40DB50 (Area75_Choices[1]): answer 0: message 0xFFFF, the tail
// state 0xF, its timer 0x1E, counter 0 = 0x14; else message 0x18.
extern "C" void __cdecl Area75_ChoiceStartF(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x18);
        return;
    }
    SetMessage(0xFFFF);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0xF;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
    B(::bof3::Img(at::kCounter0.v)) = 0x14;
}

// original 0x40DB90 (Field_ModeTailKinds[30], armed by Area75_ArmTail30): by
// the s8 state 0x9039F4 through a 36-byte table and a 24-entry jump table in
// the body (0..0x23; any other: out). Each state leaves the state it ends on
// (the register the original carries to the exit: the new state, or the one it
// entered with when it waits), and every exit with that state in 6..0xE ends in
// Area75_PhaseRun (a tail jmp). States 0..4 and 0x1F..0x23 are the scene's
// fades, area changes and music; 5..0xD three rounds (the other's turn, the
// player's, the two at once) by counter 0; 0xF a timer back to 4; 0x14 / 0x15
// a round of both from a choice; 0x1E the counts' words bumped.
extern "C" void __cdecl Area75_TailPresses(void) {
    auto cl = B(::bof3::Img(AreaW0c_TailState_addr));
    switch (static_cast<signed char>(cl)) {
    case 0:
        AH_CALL(Transition_Start)(0);
        cl = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 1:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0;
        cl = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 2: {
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x41);
        AH_CALL(Field_ChangeArea)(0x4B, 0x170000, 0x190000, 0x82);
        B(::bof3::Img(AreaW1f_MusicCurrent_addr)) = 0x58;
        const unsigned char set = AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x50), 1);
        cl = 3;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        B(::bof3::Img(at::kCounter0.v)) = set != 0 ? 0x13 : 0;
        break;
    }
    case 3:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0x1F;
        AH_CALL(Transition_Start)(1);
        cl = 4;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 4:
        if (Transition_Busy != 0) break;
        cl = 5;
        B(::bof3::Img(at::kCounter0.v)) = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 5: {
        if (B(::bof3::Img(at::kCounter0.v)) != 3) break;
        AH_CALL(Area75_ResetPresses)();
        const unsigned index = B(::bof3::Img(at::kObjectA.v));
        B(::bof3::Img(AreaW1f_PressFlags_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW1f_PressFlags_addr)) | 8);
        B(::bof3::Img(AreaW1f_Phase_addr)) = 2;
        B(::bof3::Img(AreaW1f_Move_addr)) = 0;
        B(::bof3::Img(AreaW1f_Row_addr)) = 0;
        Sprite_Current = ObjectAt(index);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 1;
        cl = 6;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    }
    case 6:
        if (Word(Mem(::bof3::Img(AreaW1f_OtherCounter_addr))) > 0x578) break;
        cl = 7;
        B(::bof3::Img(AreaW1f_Phase_addr)) = 0;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 7:
        if (B(::bof3::Img(at::kCounter0.v)) != 6) break;
        cl = 8;
        B(::bof3::Img(AreaW1f_Phase_addr)) = 2;
        B(::bof3::Img(AreaW1f_Move_addr)) = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 8:
        if (Word(Mem(::bof3::Img(AreaW1f_PlayerCounter_addr))) > 0x578) break;
        cl = 9;
        B(::bof3::Img(AreaW1f_Phase_addr)) = 0;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 9: {
        if (B(::bof3::Img(at::kCounter0.v)) != 9) break;
        AH_CALL(Msg_OpenScript)(0x17);
        const auto flags = static_cast<unsigned char>(B(::bof3::Img(AreaW1f_PressFlags_addr)) | 4);
        cl = 0xA;
        Field_Request = 2;
        B(::bof3::Img(AreaW1f_PressFlags_addr)) = flags;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    }
    case 0xA:
        if (Field_Request == 2) break;
        OpenMoveA();
        cl = 0xB;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0xB:
        if (CounterGap() <= 0x96) break;
        cl = 0xC;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
        break;
    case 0xC:
        if (Word(Mem(::bof3::Img(AreaW1f_OtherCounter_addr))) > 0x47E) break;
        cl = 0xD;
        B(::bof3::Img(AreaW1f_Phase_addr)) = 0;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0xD:
        if (B(::bof3::Img(at::kCounter0.v)) != 0xC) break;
        AH_CALL(Area75_ResetPresses)();
        cl = 0xE;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0xF: {
        const auto timer = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), timer);
        if (timer != 0) break;
        cl = 4;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    }
    case 0x14: {
        if (Field_Request == 2) break;
        AH_CALL(Area75_ResetPresses)();
        const unsigned a = B(::bof3::Img(at::kObjectA.v));
        B(::bof3::Img(AreaW1f_Phase_addr)) = 2;
        B(::bof3::Img(AreaW1f_Move_addr)) = 0;
        SetWord(Mem(::bof3::Img(AreaW1f_MoveFrames_addr)), 0x78);
        Sprite_Current = ObjectAt(a);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 1;
        const unsigned b = B(::bof3::Img(AreaW1f_ObjectB_addr));
        Sprite_Current = ObjectAt(b);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 0;
        AH_CALL(Sound_PlayEffect)(0x201);
        cl = 0x15;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    }
    case 0x15:
        AH_CALL(Area75_PhaseRun)();
        cl = B(::bof3::Img(AreaW0c_TailState_addr));
        break;
    case 0x1E: {
        const unsigned a = B(::bof3::Img(at::kObjectA.v));
        cl = 0x1F;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        AddWord(ObjectAt(a) + 0x8A, 1);
        AddWord(Mem(::bof3::Img(ObjTrio_addr) + 0x12e), 1);
        break;
    }
    case 0x1F:
        if (B(::bof3::Img(at::kCounter0.v)) != 0x1A) break;
        AH_CALL(Transition_Start)(0);
        AH_CALL(Music_FadeOut)(0x20);
        cl = 0x20;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0x20:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0;
        AH_CALL(Music_FadeOutStop)(0xA);
        AH_CALL(Sound_LoadStream)(6);
        AH_CALL(Msg_OpenScript)(0x1B);
        cl = 0x21;
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0x21:
        if (Field_Request == 2) break;
        if (AH_CALL(Sound_StreamDone)() == 0) {
            cl = B(::bof3::Img(AreaW0c_TailState_addr));
            break;
        }
        cl = 0x22;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0x22:
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x41);
        AH_CALL(Field_ChangeArea)(0x4B, 0x198000, 0x1B8000, 0x83);
        cl = 0x23;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    case 0x23:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0x1F;
        AH_CALL(Transition_Start)(1);
        B(::bof3::Img(at::kCounter0.v)) = 1;
        AH_CALL(ScriptFlags_Clear40)();
        cl = 0;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = cl;
        break;
    default:   // 0xE, 0x10..0x13, 0x16..0x1D: the table's `out`; above 0x23 or negative: `ja` to it
        break;
    }
    if (static_cast<unsigned char>(cl - 6) < 9) AH_CALL(Area75_PhaseRun)();
}

// original 0x40E120 (Area_StepHook's case for area 75): unless Cond row 10's
// flag 2, with x at 0x1B0000 or less (signed) and z's high word in
// 0x38..0x3B: ScriptFlags_Set40, counter 3 = 0xA, al 1. Else al 0.
extern "C" int __cdecl Area75_StepHook(long x, long z) {
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x50), 2) != 0) return 0;
    if (x > 0x1B0000) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x38) >= 4) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kCounter3.v)) = 0xA;
    return 1;
}

// original 0x40E160 (Field_ObjectTriggers id 41; its arguments not read):
// ScriptFlags_Set40, tail kind 0x2C at state 0 with sub-kind 0xC. al 0.
extern "C" unsigned char __cdecl Area75_Trigger41(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2C;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xC;
    return 0;
}

// original 0x40E180 (called by the tail's states 5, 0xD and 0x14): the phase,
// the flags, the two press counts, the list and its position 0; both counters
// 0x5DC (the other's first).
extern "C" void __cdecl Area75_ResetPresses(void) {
    B(::bof3::Img(AreaW1f_Phase_addr)) = 0;
    B(::bof3::Img(AreaW1f_PressFlags_addr)) = 0;
    B(::bof3::Img(AreaW1f_EarlyPresses_addr)) = 0;
    B(::bof3::Img(AreaW1f_IdleFrames_addr)) = 0;
    B(::bof3::Img(AreaW1f_List_addr)) = 0;
    B(::bof3::Img(AreaW1f_ListPos_addr)) = 0;
    SetWord(Mem(::bof3::Img(AreaW1f_OtherCounter_addr)), 0x5DC);
    SetWord(Mem(::bof3::Img(AreaW1f_PlayerCounter_addr)), 0x5DC);
}

// original 0x40E1C0 (called by the tail's state 0x15, tail-jumped to by its
// exit): call Area75_Phases[the phase byte], then jmp Area75_DrawCounters. The
// original indexes the five-entry table unchecked (a phase of 5 or more calls
// through the bytes after it): ours aborts there.
extern "C" void __cdecl Area75_PhaseRun(void) {
    const unsigned phase = B(::bof3::Img(AreaW1f_Phase_addr));
    if (phase >= at::kArea75PhaseCount)
        bof3::Fatal("Area75_PhaseRun: the phase byte %u is past Area75_Phases' %u entries (the original calls through "
                    "the bytes after it)",
                    phase, at::kArea75PhaseCount);
    reinterpret_cast<area_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Area75_Phases_addr) + phase * 4)))))();
    AH_CALL(Area75_DrawCounters)();
}

// original 0x40E1E0 (Area75_Phases[1]): the move read first. The player's
// counter at 0x1F4 or less: Sound_PlayEffect(0x201), object B's animation
// Area75_MoveAnims[move * 4 + 2] if not 0 (Sprite_Current made object B, its
// +0x2A 0), phase 4. Else a deal: both press counts 0; the rhythm row from
// Area75_Rows[Rand() & 0xF]; the list position on by one, and past the list's
// count a new list from Area75_NextList[Rand() & 0xF] at position 0; the
// move = the list's pair's first byte less 1, its frames the second; object
// B's animation Area75_MoveAnims[old move * 4 + new move] if not 0;
// Sound_PlayEffect(0x201); phase 2. The list byte indexes Area75_Lists
// unchecked (the byte compare stays in .data; the pointer read past three
// faults): ours aborts there.
extern "C" void __cdecl Area75_PhaseDeal(void) {
    const unsigned prev = B(::bof3::Img(AreaW1f_Move_addr));
    if (Word(Mem(::bof3::Img(AreaW1f_PlayerCounter_addr))) <= 0x1F4) {
        AH_CALL(Sound_PlayEffect)(0x201);
        const unsigned char anim = B(::bof3::Img(Area75_MoveAnims_addr) + 2 + prev * 4);
        if (anim != 0) {
            Sprite_Current = ObjectAt(B(::bof3::Img(AreaW1f_ObjectB_addr)));
            AH_CALL(Sprite_EnsureAnimation)(anim);
            Sprite_Current[0x2A] = 0;
        }
        B(::bof3::Img(AreaW1f_Phase_addr)) = 4;
        return;
    }
    B(::bof3::Img(AreaW1f_EarlyPresses_addr)) = 0;
    B(::bof3::Img(AreaW1f_IdleFrames_addr)) = 0;
    B(::bof3::Img(AreaW1f_Row_addr)) = B(::bof3::Img(Area75_Rows_addr) + (AH_CALL(Rand)() & 0xF));
    unsigned list = B(::bof3::Img(AreaW1f_List_addr));
    auto pos = static_cast<unsigned char>(B(::bof3::Img(AreaW1f_ListPos_addr)) + 1);
    B(::bof3::Img(AreaW1f_ListPos_addr)) = pos;
    if (pos >= B(::bof3::Img(Area75_Lists_addr) + 4 + list * 8)) {
        list = B(::bof3::Img(Area75_NextList_addr) + (AH_CALL(Rand)() & 0xF));
        pos = 0;
        B(::bof3::Img(AreaW1f_List_addr)) = static_cast<unsigned char>(list);
        B(::bof3::Img(AreaW1f_ListPos_addr)) = 0;
    }
    if (list >= at::kArea75ListCount)
        bof3::Fatal("Area75_PhaseDeal: the list byte %u is past Area75_Lists' %u records (the original follows the "
                    "bytes after them as a pointer)",
                    list, at::kArea75ListCount);
    const unsigned char* const pair =
        Mem(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Area75_Lists_addr) + list * 8)))) + pos * 2;
    const auto move = static_cast<unsigned char>(pair[0] - 1);
    B(::bof3::Img(AreaW1f_Move_addr)) = move;
    SetWord(Mem(::bof3::Img(AreaW1f_MoveFrames_addr)), pair[1]);
    const unsigned char anim = B(::bof3::Img(Area75_MoveAnims_addr) + move + prev * 4);
    if (anim != 0) {
        Sprite_Current = ObjectAt(B(::bof3::Img(AreaW1f_ObjectB_addr)));
        AH_CALL(Sprite_EnsureAnimation)(anim);
        Sprite_Current[0x2A] = 0;
    }
    AH_CALL(Sound_PlayEffect)(0x201);
    B(::bof3::Img(AreaW1f_Phase_addr)) = 2;
}

// original 0x40E330 (Area75_Phases[2]): by the move - 0: Area75_OtherPress and
// Area75_EarlyPress; 1: Area75_PlayerPress; 2: Area75_OtherPress and
// Area75_PlayerPress; any other: Area75_EarlyPress. Then the move's frames
// less 1; at 0, phase 1 unless the flags' bit 8 (then the frames 0x2710).
// Without bit 8: more than 3 presses out of turn, more than 0x1E idle frames
// or the counters more than 0xC8 apart: phase 3 - unless DIV-0188's switch
// is on (Cheats_MinigameNoFail): then the pull is never judged missed and
// runs until the player's counter reaches the mark (Area75_PhaseDeal).
extern "C" void __cdecl Area75_PhasePlay(void) {
    switch (B(::bof3::Img(AreaW1f_Move_addr))) {
    case 0:
        AH_CALL(Area75_OtherPress)();
        AH_CALL(Area75_EarlyPress)();
        break;
    case 1: AH_CALL(Area75_PlayerPress)(); break;
    case 2:
        AH_CALL(Area75_OtherPress)();
        AH_CALL(Area75_PlayerPress)();
        break;
    default: AH_CALL(Area75_EarlyPress)(); break;
    }
    const auto frames = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(AreaW1f_MoveFrames_addr))) - 1);
    SetWord(Mem(::bof3::Img(AreaW1f_MoveFrames_addr)), frames);
    const unsigned char flags = B(::bof3::Img(AreaW1f_PressFlags_addr));
    if (frames == 0) {
        if ((flags & 8) == 0)
            B(::bof3::Img(AreaW1f_Phase_addr)) = 1;
        else
            SetWord(Mem(::bof3::Img(AreaW1f_MoveFrames_addr)), 0x2710);
    }
    if (flags & 8) return;
    if (Cheats_MinigameNoFail()) return;
    if (B(::bof3::Img(AreaW1f_EarlyPresses_addr)) > 3 || B(::bof3::Img(AreaW1f_IdleFrames_addr)) > 0x1E || CounterGap() > 0xC8) B(::bof3::Img(AreaW1f_Phase_addr)) = 3;
}

// original 0x40E3C0 (Area75_Phases[3]): Cond row 10's flag 1 set, the tail
// state 0x1E.
extern "C" void __cdecl Area75_PhaseMissed(void) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x50), 1);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1E;
}

// original 0x40E3E0 (Area75_Phases[4]): story flag 0x41 cleared; with
// Cond_ByteFA at 0xA: Cond row 10's flag 0 set; the tail kind and state 0,
// the run step 0; object B's +0x83 = 4 and word +0x8A = 0; MoveScript_Var7 5;
// object A's +0x83 = 5 and word +0x8A = 0; Party_DropIn(5); counter 0 = 0x1E.
extern "C" void __cdecl Area75_PhaseReached(void) {
    AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x41);
    if (Cond_ByteFA != 0xA) return;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x50), 0);
    const unsigned b = B(::bof3::Img(AreaW1f_ObjectB_addr));
    const unsigned a = B(::bof3::Img(at::kObjectA.v));
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
    unsigned char* const object_b = ObjectAt(b);
    auto* const tObject_b = rv::As<records::SpriteObject>(object_b);
    object_b[0x83] = 4;
    tObject_b->script_position = 0;
    unsigned char* const object_a = ObjectAt(a);
    auto* const tObject_a = rv::As<records::SpriteObject>(object_a);
    MoveScript_Var7 = 5;
    object_a[0x83] = 5;
    tObject_a->script_position = 0;
    AH_CALL(Party_DropIn)(5);
    B(::bof3::Img(at::kCounter0.v)) = 0x1E;
}

// original 0x40E480 (called by Area75_PhasePlay for moves 0 and 2): the
// other's press when Area75_Rhythm[row * 15 + frames % 15] is not 0 (the
// table read unchecked; it stays in .data): Sound_PlayEffect(0x202); the
// other's counter less 4 + Rand() % 2; its effect's dword +0xC less 1; object
// A made Sprite_Current, its +0x4A = 1; jmp Sprite_ScriptTick (its answer the
// caller's to ignore).
extern "C" void __cdecl Area75_OtherPress(void) {
    const unsigned rem = Word(Mem(::bof3::Img(AreaW1f_MoveFrames_addr))) % 15;
    if (B(::bof3::Img(Area75_Rhythm_addr) + rem + B(::bof3::Img(AreaW1f_Row_addr)) * 15u) == 0) return;
    AH_CALL(Sound_PlayEffect)(0x202);
    const int r = RandHalf();
    DropCounter(::bof3::Img(AreaW1f_OtherCounter_addr), r);
    DrainEffect(B(::bof3::Img(at::kOtherEffect.v)));
    Sprite_Current = ObjectAt(B(::bof3::Img(at::kObjectA.v)));
    Sprite_Current[0x4A] = 1;
    AH_CALL(Sprite_ScriptTick)();
}

// original 0x40E520 (called by Area75_PhasePlay for moves 1 and 2): the
// button (Input_Pressed bit 0x20): Sound_PlayEffect(0x202); the player's
// counter less 4 + Rand() % 2; Sprite_Current the leader's record, its +0x4A
// = 1; the player's effect's +0xC less 1; Sprite_ScriptTick; the idle frames
// 0. No button: the idle frames + 1.
extern "C" void __cdecl Area75_PlayerPress(void) {
    if (!Pressed20()) {
        B(::bof3::Img(AreaW1f_IdleFrames_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW1f_IdleFrames_addr)) + 1);
        return;
    }
    AH_CALL(Sound_PlayEffect)(0x202);
    const int r = RandHalf();
    const unsigned slot = B(::bof3::Img(AreaW1f_PlayerEffect_addr));
    DropCounter(::bof3::Img(AreaW1f_PlayerCounter_addr), r);
    Sprite_Current = Mem(::bof3::Img(ObjTrio_addr));
    B(::bof3::Img(ObjTrio_addr) + 0x4a) = 1;
    DrainEffect(slot);
    AH_CALL(Sprite_ScriptTick)();
    B(::bof3::Img(AreaW1f_IdleFrames_addr)) = 0;
}

// original 0x40E5A0 (called by Area75_PhasePlay for move 0 and moves past 2):
// unless the flags' bit 8, the button: Sprite_Current the leader's record, its
// +0x4A = 1, the player's effect's +0xC less 1, Sprite_ScriptTick, the presses
// out of turn + 1.
extern "C" void __cdecl Area75_EarlyPress(void) {
    if (B(::bof3::Img(AreaW1f_PressFlags_addr)) & 8) return;
    if (!Pressed20()) return;
    Sprite_Current = Mem(::bof3::Img(ObjTrio_addr));
    const unsigned slot = B(::bof3::Img(AreaW1f_PlayerEffect_addr));
    B(::bof3::Img(ObjTrio_addr) + 0x4a) = 1;
    DrainEffect(slot);
    AH_CALL(Sprite_ScriptTick)();
    B(::bof3::Img(AreaW1f_EarlyPresses_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW1f_EarlyPresses_addr)) + 1);
}

// original 0x40E5F0 (jmp'd to by Area75_PhaseRun): unless the flags' bit 1,
// each counter in a window (Area75_DrawWindow at (0x18, 0x20) and (0x90,
// 0x20), 0x54 x 0x11) and printed as (word / 100, word % 100), each a byte,
// by Crt_sprintf(0x904BA0, Area75_CounterFormat, ...) and Text_DrawAt at
// (0x1E, 0x22) / (0x96, 0x22), 6 characters, colour 2 when the counters are
// more than 0x96 apart, else 0. With the flags' bit 4 the text is drawn only
// on frames whose counter has a bit of 0xC. The original pushes the colour as
// a dword whose upper bytes are its own stack's (Text_DrawString reads the
// low byte only).
extern "C" void __cdecl Area75_DrawCounters(void) {
    const bool show = (B(::bof3::Img(AreaW1f_PressFlags_addr)) & 4) ? (Frame_Counter & 0xC) != 0 : true;
    const unsigned first = Word(Mem(::bof3::Img(AreaW1f_PlayerCounter_addr)));
    const unsigned color = CounterGap() <= 0x96 ? 0u : 2u;
    if (B(::bof3::Img(AreaW1f_PressFlags_addr)) & 1) return;
    unsigned char* const text = Mem(::bof3::Img(at::kTextBuffer.v));
    const char* const format = reinterpret_cast<const char*>(Mem(::bof3::Img(Area75_CounterFormat_addr)));
    AH_CALL(Area75_DrawWindow)(0x18, 0x20, 0x54, 0x11, 0);
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), format, static_cast<unsigned char>(first / 100),
                         static_cast<unsigned char>(first % 100));
    if (show) AH_CALL(Text_DrawAt)(0x1E, 0x22, static_cast<int>(color), 6, text);
    const unsigned second = Word(Mem(::bof3::Img(AreaW1f_OtherCounter_addr)));
    AH_CALL(Area75_DrawWindow)(0x90, 0x20, 0x54, 0x11, 0);
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), format, static_cast<unsigned char>(second / 100),
                         static_cast<unsigned char>(second % 100));
    if (show) AH_CALL(Text_DrawAt)(0x96, 0x22, static_cast<int>(color), 6, text);
}

// original 0x40E750 (called by Area75_DrawCounters and by area 42's timer tail
// 0x406A30): a window of (x, y, w, h), `flag` choosing the shade (0xFF, else
// 0xAC), the texture page (0x2F, else 0xF) and the CLUT row: a draw-mode
// primitive (its RECT (0, 0xF0, 0x10, 0x10) in the 8 bytes before it), four
// semi-transparent FT4 quads with float corners - the left edge 2 wide, the
// middle in two halves of (w + 1 - 4) >> 1 (the right one a pixel wider for an
// odd w + 1), the right edge 2 wide, each the height h + 1 with its corners
// cut by 2 / 3 / 1 as the texture's - each with Gpu_GetClut(flag * 32 + 0x10,
// 0x1E1); a second draw-mode primitive (RECT (0, 0, 0x100, 0x100)); then
// Menu_DrawOutline(x + 2, y + 2, w - 4, h - 4, flag). x, y and w + 1, h + 1
// are taken as words for the corners, as dwords for the outline.
extern "C" void __cdecl Area75_DrawWindow(int x, int y, int w, int h, int flag) {
    const auto flag8 = static_cast<unsigned char>(flag);
    const unsigned char shade = flag8 != 0 ? 0xFF : 0xAC;
    const unsigned tpage = (flag8 != 0 ? 0x20u : 0u) | 0xFu;
    const auto clut_x = static_cast<int>((static_cast<unsigned>(flag) & 0xFF) << 5) + 0x10;
    // the first draw mode
    unsigned char* const rect0 = Gfx_PacketNext;
    Gfx_PacketNext = rect0 + 8;
    SetWord(rect0, 0);
    SetWord(rect0 + 2, 0xF0);
    SetWord(rect0 + 4, 0x10);
    SetWord(rect0 + 6, 0x10);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage,
                             Low32(rect0));
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    const auto w1 = static_cast<std::uint32_t>(w) + 1;
    const auto h1 = static_cast<std::uint32_t>(h) + 1;
    const int xs = static_cast<int>(static_cast<std::uint32_t>(x) & 0xFFFF);
    const int ys = static_cast<int>(static_cast<std::uint32_t>(y) & 0xFFFF);
    const int h1w = static_cast<int>(h1 & 0xFFFF);
    const auto hb = static_cast<unsigned char>(h1);
    const float fx = static_cast<float>(xs);
    const float fy = static_cast<float>(ys);
    const float fx2 = static_cast<float>(xs + 2);
    const float fyh = static_cast<float>(ys + h1w);
    const float fy2 = fy + 2.0f;     // + the constant 0x5C41C0
    const float fyh3 = fyh - 3.0f;   // - the constant 0x5C41BC
    // the left edge
    unsigned char* p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    p[0x14] = 0;
    SetFloat(p + 8, fx);
    p[0x24] = 2;
    p[0x25] = 0;
    p[0x34] = 0;
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x18, fx2);
    SetFloat(p + 0x28, fx);
    SetFloat(p + 0x38, fx2);
    SetFloat(p + 0x3C, fyh);
    p[0x45] = hb;
    p[0x35] = static_cast<unsigned char>(hb - 3);
    SetFloat(p + 0xC, fy2);
    p[0x44] = 2;
    p[4] = shade;
    SetFloat(p + 0x2C, fyh3);
    p[5] = shade;
    p[6] = shade;
    p[0x15] = 2;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the middle's left half
    const int ww = static_cast<int>(w1 & 0xFFFF);
    const int half = (ww - 4) >> 1;
    const int odd = static_cast<int>(w1 & 1);
    const int hs = static_cast<short>(half);
    const auto hu = static_cast<unsigned char>(half);
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const float fxm = static_cast<float>(hs + xs + 2);
    SetFloat(p + 8, fx2);
    SetFloat(p + 0xC, fy);
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x28, fx2);
    SetFloat(p + 0x3C, fyh);
    SetFloat(p + 0x18, fxm);
    SetFloat(p + 0x38, fxm);
    SetFloat(p + 0x2C, fyh);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = hu;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = hu;
    p[0x45] = hb;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the middle's right half
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const float fxr = static_cast<float>(xs + odd + hs * 2 + 2);
    const auto ur = static_cast<unsigned char>(odd + hu);
    SetFloat(p + 8, fxm);
    SetFloat(p + 0xC, fy);
    p[0x14] = 0;
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x28, fxm);
    SetFloat(p + 0x2C, fyh);
    SetFloat(p + 0x18, fxr);
    SetFloat(p + 0x38, fxr);
    SetFloat(p + 0x3C, fyh);
    p[0x15] = 0;
    p[0x24] = ur;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = ur;
    p[0x45] = hb;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the right edge
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const int xr = xs + ww;
    const float fxe = static_cast<float>(xr);
    const float fxe2 = static_cast<float>(xr - 2);
    SetFloat(p + 0xC, fy);
    SetFloat(p + 8, fxe2);
    SetFloat(p + 0x18, fxe);
    SetFloat(p + 0x28, fxe2);
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    SetFloat(p + 0x38, fxe);
    SetFloat(p + 0x3C, fyh3);
    SetFloat(p + 0x2C, fyh - 1.0f);   // - the constant 0x5C41B8
    SetFloat(p + 0x1C, fy2);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = 2;
    p[0x34] = 0;
    p[0x44] = 2;
    p[0x25] = 2;
    p[0x35] = static_cast<unsigned char>(hb - 1);
    p[0x45] = static_cast<unsigned char>(hb - 3);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the second draw mode, then the outline
    unsigned char* const rect1 = Gfx_PacketNext;
    Gfx_PacketNext = rect1 + 8;
    SetWord(rect1, 0);
    SetWord(rect1 + 2, 0);
    SetWord(rect1 + 4, 0x100);
    SetWord(rect1 + 6, 0x100);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage,
                             Low32(rect1));
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_CALL(Menu_DrawOutline)(static_cast<int>(static_cast<std::uint32_t>(x) + 2),
                              static_cast<int>(static_cast<std::uint32_t>(y) + 2), static_cast<int>(w1 - 5),
                              static_cast<int>(h1 - 5), flag);
}

// original 0x40EB90 (Area_StepHook's case for area 0x4C, called with no
// arguments: it reads none, event_ops.cpp): with the mode tail kind at 5,
// the kind, its state and the byte 0x9039F1 cleared. Answers 0 in al.
extern "C" unsigned char __cdecl Area76_StepDisarmTail5(void) {
    if (B(::bof3::Img(AreaW0a_TailKind_addr)) == 5) {
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW2a_ModeByte_addr)) = 0;
    }
    return 0;
}

void Area75_Inject() {
    BOF3_INJECT(Area75_ArmTail30);
    BOF3_INJECT(Area75_MarkObjectB);
    BOF3_INJECT(Area75_MarkObjectA);
    BOF3_INJECT(Area75_ShowEffects);
    BOF3_INJECT(Area75_PoseObject5);
    BOF3_INJECT(Area75_SpawnFive);
    BOF3_INJECT(Area75_FlyRun);
    BOF3_INJECT(Area75_FlyBegin);
    BOF3_INJECT(Area75_FlyStep);
    BOF3_INJECT(Area75_DrainEffects);
    BOF3_INJECT(Area75_FollowRun);
    BOF3_INJECT(Area75_FollowBegin);
    BOF3_INJECT(Area75_FollowStep);
    BOF3_INJECT(Area75_FollowKind2Z);
    BOF3_INJECT(Area75_ChoiceStart14);
    BOF3_INJECT(Area75_ChoiceStartF);
    BOF3_INJECT(Area75_TailPresses);
    BOF3_INJECT(Area75_StepHook);
    BOF3_INJECT(Area75_Trigger41);
    BOF3_INJECT(Area75_ResetPresses);
    BOF3_INJECT(Area75_PhaseRun);
    BOF3_INJECT(Area75_PhaseDeal);
    BOF3_INJECT(Area75_PhasePlay);
    BOF3_INJECT(Area75_PhaseMissed);
    BOF3_INJECT(Area75_PhaseReached);
    BOF3_INJECT(Area75_OtherPress);
    BOF3_INJECT(Area75_PlayerPress);
    BOF3_INJECT(Area75_EarlyPress);
    BOF3_INJECT(Area75_DrawCounters);
    BOF3_INJECT(Area75_DrawWindow);
    BOF3_INJECT(Area76_StepDisarmTail5);
    // DIV-0188 guards the rope pull's judgement here; Capcom's
    // Area75_PhasePlay would judge as before and the cheat would be silently
    // half off.
    if (Cheats_MinigameNoFail() && !bof3::IsEnabled(bof3::addr::Area75_PhasePlay))
        bof3::Fatal("BOF3X_NO_FAIL=1 needs Area75_PhasePlay ours, and it is disabled");
}

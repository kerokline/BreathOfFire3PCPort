// Moved here by tools/move_funcs.py from area_w1b.cpp (docs/source-layout-round.md).
#include "game/area/area_42.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char& B(U address) { return move_script::At(address)[0]; }

signed char TailState() { return AreaW0c_TailState; }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

unsigned char* ActiveMember() { return Field_ActiveMember; }

// Area 42's and 44's own functions another of them calls directly, by their
// original addresses (each is patched to ours; the fuzz stands a recorder in).
constexpr ::bof3::image::CodeVa kCheckAll42{bof3::addr::Area42_CheckAll};

using CheckAllFn = unsigned char (__cdecl*)();

// Area 42's handlers 2..5 (0x406710, 0x406780, 0x4067F0, 0x406860; one shape,
// the flag 0x12..0x15): Field_ActiveMember's +0x80 bit 0 cleared; only when
// Character_IndexById[the leader's +0x89] is 1 and the flag is not yet
// set: the flag set, Area42_CheckAll (its answer not read), and unless flag
// 0xB is set, Field_ActiveMember's word +0x8A (read again) + 1.
void Touch42(unsigned flag) {
    ActiveMember()[0x80] = static_cast<unsigned char>(ActiveMember()[0x80] & 0xFE);
    if (Character_IndexById[ObjTrio[0].char_id] != 1) return;
    if (AH_CALL(Flags_Test)(Bank(), flag) != 0) return;
    AH_CALL(Flags_Set)(Bank(), flag);
    AH_AT(CheckAllFn, kCheckAll42.v)();
    if (AH_CALL(Flags_Test)(Bank(), 0xB) != 0) return;
    unsigned char* const m = ActiveMember();
    rv::Store<unsigned>(rv::As<records::SpriteObject>(m)->script_position, rv::As<records::SpriteObject>(m)->script_position + 1u);
}

// The countdown's two draws (states 0 and 10 of Area42_TimerTail): the
// window 0x40E750(0x7C, 0x2E, 0x48, 0x11, 0); with `jitter`, Rand() & 3 (after
// the window, as the original calls it); then Crt_sprintf(0x904BA0, the
// format 0x5F6308, seconds, hundredths) with seconds = the word / 30 and
// hundredths = (the word % 30) * 10 / 3 (plus the jitter), both as words.
// Answers the seconds' low word.
unsigned FormatTime(bool jitter) {
    const unsigned w = Word(At(::bof3::Img(at::kTailTimer.v)));
    const unsigned seconds = w / 30u;
    const unsigned hundredths = (w % 30u) * 10u / 3u;
    AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned), area_w1b::kDrawWindow.v)(0x7C, 0x2E, 0x48, 0x11, 0);
    const unsigned extra = jitter ? static_cast<unsigned>(AH_CALL(Rand)()) & 3u : 0u;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextBuffer.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kA42Format.v))),
                         seconds & 0xFFFF, extra + (hundredths & 0xFFFF));
    return seconds & 0xFFFF;
}

void DrawTime(unsigned colour) {
    AH_CALL(Text_DrawAt)(0x82, 0x30, static_cast<int>(colour), 5, At(::bof3::Img(at::kTextBuffer.v)));
}

// An effect of kind 0x13 (a camera turn, CameraTurn_Run) at Effect_FindFree's
// slot, kept in 0x9039F5: +0x64 = target, +0x68 / +0x6C = Camera_Angles[1] /
// [2] sign-extended, +9 = 0x10 frames.
void SpawnTurn(std::int32_t target) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    AreaW0c_TailSub = slot;
    if (slot == 0xFF) return;
    const std::int32_t a1 = Camera_Angles[1];
    const std::int32_t a2 = S16(At(::bof3::Img(Cond_AngleFB_addr)));
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x13;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[0], target);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[1], a1);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[2], a2);
    e[9] = 0x10;
}

// Kind 7's two ends: F3, F4 and the countdown word zeroed.
void EndTail7() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
    SetWord(At(::bof3::Img(at::kTailTimer.v)), 0);
}

}  // namespace

// original 0x406650 (area 42's choice 0, the descriptor 0x5F6290's +0x34, and
// its handler 6 - the choice table is the handler array's tail; PSX
// 0x801F3D90): message = Area42_Messages 0x5F62D4 by the s8 choice
// (unchecked).
extern "C" void __cdecl Area42_ChoiceMessage(void) {
    const auto choice = AreaW0c_ChoiceAnswer;
    SetWord(At(::bof3::Img(at::kMessage.v)), Word(At(::bof3::Img(Area42_Messages_addr) + static_cast<U>(choice * 2))));
}

// original 0x406670 (area 42's handler 0, Area42_Handlers 0x5F6274; PSX
// 0x801F3DBC): Field_ActiveMember's +0x80 bit 0 cleared; then, only when
// Character_IndexById[the leader's +0x89] is 1 and neither story flag 0xB
// nor 0x11 is set: flag 0xB set, Field_ScriptFlags bit 5, and the mode tail
// armed - kind 7 (0x9039F3), state 0xA, the countdown word 0x9039F6 = 0x384.
extern "C" void __cdecl Area42_StartTimer(void) {
    ActiveMember()[0x80] = static_cast<unsigned char>(ActiveMember()[0x80] & 0xFE);
    if (Character_IndexById[ObjTrio[0].char_id] != 1) return;
    if (AH_CALL(Flags_Test)(Bank(), 0xB) != 0) return;
    if (AH_CALL(Flags_Test)(Bank(), 0x11) != 0) return;
    AH_CALL(Flags_Set)(Bank(), 0xB);
    reinterpret_cast<unsigned char*>(&Field_ScriptFlags)[0] |= 0x20;   // a byte or
    AreaW0a_TailKind = 7;
    AreaW0c_TailState = 0xA;
    SetWord(At(::bof3::Img(at::kTailTimer.v)), 0x384);
}

// original 0x4066F0 (area 42's handler 1; PSX 0x801F3E80): Flags_Clear(story
// flags, the byte Field_ActiveMember +0xA0 plus 0x10, a byte).
extern "C" void __cdecl Area42_ClearMemberFlag(void) {
    AH_CALL(Flags_Clear)(Bank(), static_cast<unsigned char>(ActiveMember()[0xA0] + 0x10));
}

// original 0x406710 (area 42's handler 2; PSX 0x801F3EB8): Touch42 of flag 0x12.
extern "C" void __cdecl Area42_Touch12(void) { Touch42(0x12); }

// original 0x406780 (handler 3; PSX 0x801F3F6C): flag 0x13.
extern "C" void __cdecl Area42_Touch13(void) { Touch42(0x13); }

// original 0x4067F0 (handler 4; PSX 0x801F4020): flag 0x14.
extern "C" void __cdecl Area42_Touch14(void) { Touch42(0x14); }

// original 0x406860 (handler 5; PSX 0x801F40D4): flag 0x15.
extern "C" void __cdecl Area42_Touch15(void) { Touch42(0x15); }

// original 0x4068D0 (called by the four handlers above; PSX 0x801F4188):
// al 0 unless story flags 0x12..0x15 are all set and 0x11 is not. Then
// ScriptFlags_Set40; q = the countdown word / 30 (its seconds); the tail to
// kind 7 state 0 with 0x9039F5 = 0x3C (the time shown for 60 frames); the
// rank 0x675A00 = 2 for q's low byte above 5, 1 at 5, else 0. Then the first
// of the 30 field objects with +0 bit 0, +6 == 9 and +5 in 0x5C..0x5E whose
// flag rank + 0x5C of the bank 0x9040CC is set (the rank re-read from
// 0x675A00 after each flag that is not): Sprite_Current made it,
// Sprite_SetAnimation(1), Sprite_Current back to Field_ActiveMember (read
// after the call). al 1 either way.
extern "C" unsigned char __cdecl Area42_CheckAll(void) {
    for (unsigned flag = 0x12; flag <= 0x15; ++flag)
        if (AH_CALL(Flags_Test)(Bank(), flag) == 0) return 0;
    if (AH_CALL(Flags_Test)(Bank(), 0x11) != 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    const unsigned seconds = Word(At(::bof3::Img(at::kTailTimer.v))) / 30u;
    AreaW0a_TailKind = 7;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 0x3C;
    const auto low = static_cast<unsigned char>(seconds);
    unsigned char rank = low > 5 ? 2 : (low == 5 ? 1 : 0);
    Area42_Rank = rank;
    for (unsigned k = 0; k < 30; ++k) {
        unsigned char* const o = rv::Bytes(&Sprite_Objects[k]);
        if ((o[0] & 1) == 0 || o[6] != 9 || static_cast<unsigned char>(o[5] - 0x5C) >= 3) continue;
        if (AH_CALL(Flags_Test)(At(::bof3::Img(at::kFlagsCC.v)), static_cast<unsigned char>(rank + 0x5C)) != 0) {
            Sprite_Current = o;
            AH_CALL(Sprite_SetAnimation)(1);
            Sprite_Current = Field_ActiveMember;
            return 1;
        }
        rank = Area42_Rank;
    }
    return 1;
}

// original 0x406A30 (Field_ModeTailKinds slot 7, armed by Area42_StartTimer
// and Area42_CheckAll; PSX 0x801F4A00): a switch on the s8 0x9039F4 (0..10,
// a jump table of 11 inside the function).
//    0  with 0x9039F5 not 0: the time drawn (colour 2 below ten seconds, else
//       0), 0x9039F5 (read again) - 1. At 0: as 1.
//    1  Field_Kind2X / Z = (0x15, 0x19) cells, MoveScript_F3Divisor 0x40,
//       state 2.
//    2  once Field_Kind2Hold is 0: MoveScript_F3Divisor 0, a camera turn to
//       -770, state 3.
//    3  once the turn's effect record +0 bit 0 is clear: story flag 0x11,
//       state 9.
//    4  a camera turn to -682 (no hold test), state 5.
//    5  once it is done: Field_Kind2X / Z = the leader's position,
//       MoveScript_F3Divisor 0x40, state 6.
//    6  once Field_Kind2Hold is 0: story flag 0xB cleared,
//       ScriptFlags_Clear40, Field_ScriptFlags bit 5 cleared,
//       MoveScript_F3Divisor 0, the tail ended.
//   10  the countdown: the time drawn with Rand() & 3 added to the
//       hundredths, colour (the word's low byte, read again, >> 1) & 2 below
//       ten seconds; the word (read again) - 1, or at 0 story flags 0xB,
//       0x12..0x15 cleared, Field_ScriptFlags bit 5 cleared, the tail ended.
//  7..9 and any other: nothing.
extern "C" void __cdecl Area42_TimerTail(void) {
    switch (TailState()) {
    case 0:
        if (AreaW0c_TailSub != 0) {
            const unsigned seconds = FormatTime(false);
            DrawTime(seconds < 10 ? 2u : 0u);
            AreaW0c_TailSub = static_cast<unsigned char>(AreaW0c_TailSub - 1);
            return;
        }
        [[fallthrough]];
    case 1:
        Field_Kind2X = 0x150000;
        Field_Kind2Z = 0x190000;
        MoveScript_F3Divisor = 0x40;
        AreaW0c_TailState = 2;
        return;
    case 2:
        if (Field_Kind2Hold != 0) return;
        MoveScript_F3Divisor = 0;
        SpawnTurn(-770);
        AreaW0c_TailState = 3;
        return;
    case 3:
        if ((EffectAt(AreaW0c_TailSub)[0] & 1) != 0) return;
        AH_CALL(Flags_Set)(Bank(), 0x11);
        AreaW0c_TailState = 9;
        return;
    case 4:
        SpawnTurn(-682);
        AreaW0c_TailState = 5;
        return;
    case 5:
        if ((EffectAt(AreaW0c_TailSub)[0] & 1) != 0) return;
        Field_Kind2X = ObjTrio[0].x;
        Field_Kind2Z = ObjTrio[0].z;
        MoveScript_F3Divisor = 0x40;
        AreaW0c_TailState = 6;
        return;
    case 6:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(Flags_Clear)(Bank(), 0xB);
        AH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFDF);
        MoveScript_F3Divisor = 0;
        EndTail7();
        return;
    case 10: {
        const unsigned seconds = FormatTime(true);
        DrawTime(seconds < 10 ? (B(::bof3::Img(at::kTailTimer.v)) >> 1) & 2u : 0u);
        if (Word(At(::bof3::Img(at::kTailTimer.v))) != 0) {
            SetWord(At(::bof3::Img(at::kTailTimer.v)), Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
            return;
        }
        AH_CALL(Flags_Clear)(Bank(), 0xB);
        AH_CALL(Flags_Clear)(Bank(), 0x12);
        AH_CALL(Flags_Clear)(Bank(), 0x13);
        AH_CALL(Flags_Clear)(Bank(), 0x14);
        AH_CALL(Flags_Clear)(Bank(), 0x15);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFDF);
        EndTail7();
        return;
    }
    default:
        return;
    }
}

// original 0x406DE0 (Area_StepHook's handler for area 42; PSX 0x801F4AF4):
// (x, z) 16.16 positions. z exactly 0x308000 with x's high word 0x7B or 0x7C,
// or z exactly 0x368000 with x's high word 0x1D..0x1F: story flags 0x11, 0xB,
// 0x12..0x15 cleared, Field_ScriptFlags bit 5 cleared, and when the tail kind
// (read after the calls) is 7 the tail ended. al 0 always.
extern "C" unsigned char __cdecl Area42_StepHook(unsigned x, unsigned z) {
    const auto column = static_cast<unsigned short>(x >> 16);
    if (z == 0x308000) {
        if (static_cast<unsigned short>(column - 0x7B) >= 2) return 0;
    } else {
        if (z != 0x368000) return 0;
        if (static_cast<unsigned short>(column - 0x1D) >= 3) return 0;
    }
    AH_CALL(Flags_Clear)(Bank(), 0x11);
    AH_CALL(Flags_Clear)(Bank(), 0xB);
    AH_CALL(Flags_Clear)(Bank(), 0x12);
    AH_CALL(Flags_Clear)(Bank(), 0x13);
    AH_CALL(Flags_Clear)(Bank(), 0x14);
    AH_CALL(Flags_Clear)(Bank(), 0x15);
    const unsigned char kind = static_cast<unsigned char>(AreaW0a_TailKind);
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFDF);
    if (kind == 7) EndTail7();
    return 0;
}

// original 0x406E90 (area 42's init, the descriptor's +0x40; PSX 0x801F5070).
// With Cond_ByteFD 0: unless story flag 0x11 is set, the rank 0x675A00 = the
// first of 0..2 whose flag 0x5C + i of the bank 0x9040CC is clear (2 when all
// three are set); then a free field object from 0x57CD90, its index to the
// word 0x903850 (zero-extended), and unless 0xFF EventOp_9x of
// Area42_Placements[rank] (13 bytes each, the rank re-read, unchecked). Then
// with Cond_ByteFD (read again) 1: Effect_FindFree to the byte 0x903850; a
// slot: its record (the slot re-read as the dword's low byte) +0 = 1, kind
// +5 = 0x28, +6 = 2, (+0xC, +0x10) = (0x418000, 0x108000), (+0x18, +0x1C) =
// (0x4D8000, 0x108000).
extern "C" void __cdecl Area42_Init(void) {
    if (Cond_ByteFD == 0) {
        if (AH_CALL(Flags_Test)(Bank(), 0x11) == 0) {
            unsigned i = 0;
            for (; i < 3; ++i) {
                if (AH_CALL(Flags_Test)(At(::bof3::Img(at::kFlagsCC.v)), static_cast<unsigned char>(i + 0x5C)) == 0) {
                    Area42_Rank = static_cast<unsigned char>(i);
                    break;
                }
            }
            if (i == 3) Area42_Rank = 2;
        }
        const auto found = static_cast<unsigned char>(AH_AT(unsigned (__cdecl*)(), area_w1b::kFreeObject.v)());
        SetWord(At(::bof3::Img(at::kFoundSlot_addr.v)), found);
        if (found != 0xFF) AH_CALL(EventOp_9x)(At(::bof3::Img(Area42_Placements_addr) + Area42_Rank * 13u));
    }
    if (Cond_ByteFD != 1) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    at::kFoundSlot = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(at::kFoundSlot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x28;
    e[6] = 2;
    tE->step_x = 0x418000;
    tE->step_z = 0x108000;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_18, 0x4D8000);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_1c, 0x108000);
}

void Area42_Inject() {
    BOF3_INJECT(Area42_ChoiceMessage);
    BOF3_INJECT(Area42_StartTimer);
    BOF3_INJECT(Area42_ClearMemberFlag);
    BOF3_INJECT(Area42_Touch12);
    BOF3_INJECT(Area42_Touch13);
    BOF3_INJECT(Area42_Touch14);
    BOF3_INJECT(Area42_Touch15);
    BOF3_INJECT(Area42_CheckAll);
    BOF3_INJECT(Area42_TimerTail);
    BOF3_INJECT(Area42_StepHook);
    BOF3_INJECT(Area42_Init);
}

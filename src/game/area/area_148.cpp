// Moved here by tools/move_funcs.py from area_w3f.cpp, area_w3g.cpp (docs/source-layout-round.md).
#include "game/area/area_148.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area/area_148_callees.h"
#include "game/area_harness.h"
#include "game/area_w3f_callees.h"
#include "game/area_w3g_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_148::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

using U = std::uint32_t;

using area_harness::Handler;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* PartyRecord(U index) { return At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride); }

unsigned char* EffectAt(U slot) { return At(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

unsigned char& B(U address) { return move_script::At(address)[0]; }

void Disarm() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
}

// The tails' u16 frame counter less one: true when it reached 0.
bool TimerDone() {
    const auto t = static_cast<std::uint16_t>(Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
    SetWord(At(::bof3::Img(at::kTailTimer.v)), t);
    return t == 0;
}

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// `fld dword [a]; fsub dword [b]` then the CRT's _ftol 0x5B9550 (round toward
// zero set in a copy of the control word for one fistp to 64 bits, the word
// put back): the low dword. The difference is rounded to the precision the
// control word holds at the call, as the original's; Math_Ratan2
// (move_cmds.cpp) is the same idiom.
U FtolDiff(const unsigned char* a, const unsigned char* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fld dword [a]` then _ftol: the low dword.
U Ftol(const unsigned char* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fild dword` then `fstp dword`: an int to a float, rounded to nearest.
void StoreFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

float FloatOf(std::int32_t v) { return static_cast<float>(v); }

// Handlers 4 and 5 (choices 6 and 7): the first of four member ids (the outer
// loop) that party record 0 .. Field_MemberCount - 1 has at +0x89 (the inner
// loop, the count read once, unsigned, records past the third unchecked):
// Msg_OpenScript(its message) and Field_Request = 2. None: nothing.
void MessageByMember(U ids, U messages) {
    const unsigned count = Field_MemberCount;
    for (U i = 0; i < 4; ++i) {
        const unsigned char id = B(ids + i);
        for (U j = 0; j < count; ++j) {
            if (B(::bof3::Img(at::kMemberId.v) + j * at::kPartyStride) != id) continue;
            AH_CALL(Msg_OpenScript)(Word(At(messages + i * 2)));
            Field_Request = 2;
            return;
        }
    }
}

// Tail kind 31's and area 148's effect of kind 0x13: slot `slot`'s record +0
// = 1, kind +5 = 0x13, +0x64 `x`, +0x68 the camera's angle 1 (s16), +0x6C `y`,
// +9 `life`. The slot is Effect_FindFree's, not 0xFF (records past the
// twentieth are written as the original writes them).
void Kind13(U slot, U x, U y, unsigned char life) {
    const std::int32_t angle = Camera_Angles[1];
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x13;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[0], static_cast<std::int32_t>(x));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[1], angle);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[2], static_cast<std::int32_t>(y));
    e[9] = life;
}

// The beam's colour byte k of entry Sprite_Current[6] (three bytes an entry,
// unchecked; the pointer read again for each byte, as the originals do).
unsigned char BeamColour(unsigned k) { return B(::bof3::Img(Area148_BeamColours_addr) + Cur()[6] * 3u + k); }

using ProjectFn = void (__cdecl*)(const long*, unsigned char*);

using ScreenSizeFn = void (__cdecl*)(const long*, const short*, void*);

// A draw-mode packet (abr 1 on tpage 0x3C0, dtd as given) at Gfx_PacketNext,
// linked at the running effect's (+0x18, +0x1C) with size 0xC.
void DrawModeLink(int dtd) {
    const unsigned tpage = AH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFFu;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    unsigned char* const o = Cur();
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::As<records::SpriteHead>(o)->work_18), static_cast<U>(rv::As<records::SpriteHead>(o)->work_1c), 0, 0xC);
}

// A primitive linked at the running effect's (+0x18, +0x1C).
void LinkHere(unsigned size) {
    unsigned char* const o = Cur();
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::As<records::SpriteHead>(o)->work_18), static_cast<U>(rv::As<records::SpriteHead>(o)->work_1c), 0, size);
}

// (trig * r) >> 12, the product 32-bit, the shift arithmetic.
std::int32_t Scaled(int trig, std::int32_t r) {
    return static_cast<std::int32_t>(static_cast<U>(trig) * static_cast<U>(r)) >> 12;
}

}  // namespace

// original 0x422350 (area 148 +0x3C[1] = +0x34[3], area 167 +0x3C[1] =
// +0x34[6]; in area 146's block): story flag 0x46 cleared, Cond_ByteFE 0.
extern "C" void __cdecl Area146_ClearFlag46(void) {
    AH_CALL(Flags_Clear)(StoryFlags(), 0x46);
    Cond_ByteFE = 0;
}

// original 0x422370 (area 148 +0x3C[2] = +0x34[4], area 149 +0x3C[0], area
// 167 +0x3C[8] = +0x34[13]; in area 146's block): the running object's x, z,
// y = Sprite_ObjectsExtra record 0's, Sprite_Current read again for each.
extern "C" void __cdecl Area146_ToExtraObject0(void) {
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34)));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38)));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x3C)));
}

// original 0x4223A0 (handler 4 / choice 6; PSX 0x801F3A20): members 5, 2, 8, 4
// in that order - messages 0xB..0xE (Area148 tables 0x6353D8 / 0x6353DC).
extern "C" void __cdecl Area148_MemberMessageA(void) { MessageByMember(::bof3::Img(Area148_MemberIdsA_addr), ::bof3::Img(Area148_MessagesA_addr)); }

// original 0x422430 (handler 5 / choice 7; PSX 0x801F3AE4): members 4, 8, 2, 5
// - messages 0xF..0x12 (0x6353E4 / 0x6353E8).
extern "C" void __cdecl Area148_MemberMessageB(void) { MessageByMember(::bof3::Img(Area148_MemberIdsB_addr), ::bof3::Img(Area148_MessagesB_addr)); }

// original 0x4224C0 (choice 0): message 6 when the cursor byte is not 0, else 4.
extern "C" void __cdecl Area148_ChoiceMessage(void) { SetMessage(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0 ? 6u : 4u); }

// original 0x4224E0 (choice 1): the cursor 0: message 5 (stored before the
// call), Flags_Set(0x904030, 0x59); else no message (0xFFFF).
extern "C" void __cdecl Area148_ChoiceSetFlag59(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0) {
        SetMessage(0xFFFF);
        return;
    }
    SetMessage(5);
    AH_CALL(Flags_Set)(Bank(), 0x59);
}

// original 0x422510 (Field_ModeTailKinds[31]): by the s8 0x9039F4 through the
// byte table 0x422778 and the case table 0x422750 (states 0..0x15, any other
// nothing):
//   0: Party_DropIn(0), state 1.
//   1: the script byte 0x90384B at 0x18: the tail disarmed, flag 0x46 set,
//      Field_ChangeArea(0x95, 0x1C0000, 0xC0000, 0x80).
//   0xA: an effect slot (Effect_FindFree) kept in 0x9039F5; none: again next
//      frame; else state 0xB and the slot a kind-0x13 effect (-0x39A, 0x190,
//      life 0x30).
//   0xB: once that slot's record +0 bit 0 is clear (the slot byte unchecked,
//      a read): Kind2_Place(2), state 0xC.
//   0xC: the script byte at 0x20: flag 0x5A set, the timer 0x20, state 0xD.
//   0xD: the timer's end: the second effect (-0x2AA, 0x200, life 0x20),
//      state 0xE (none: the timer at 0 again next frame).
//   0xE: the script byte 0: ScriptFlags_Clear40, the tail disarmed.
//   0x14: flag 0x6A toggled, Sound_PlayEffect(0x204); the flag now set:
//      Gfx_ClutAdjust(0, 0x380, -6, -4, -4), else (0, 0x180, 0, 0, 0);
//      Draw_PassFlags 0, the timer 0xF, state 0x15.
//   0x15: the timer's end: Draw_PassFlags 0x1F, ScriptFlags_Clear40, the
//      tail disarmed.
extern "C" void __cdecl Area148_Tail31(void) {
    switch (AreaW0c_TailState) {
    case 0:
        AH_CALL(Party_DropIn)(0);
        AreaW0c_TailState = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kScriptVar.v)) != 0x18) return;
        Disarm();
        AH_CALL(Flags_Set)(Bank(), 0x46);
        AH_CALL(Field_ChangeArea)(0x95, 0x1C0000, 0xC0000, 0x80);
        return;
    case 0xA: {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        AreaW0c_TailSub = slot;
        if (slot == 0xFF) return;
        AreaW0c_TailState = 0xB;
        Kind13(slot, 0xFFFFFC66u, 0x190, 0x30);
        return;
    }
    case 0xB:
        if ((EffectAt(AreaW0c_TailSub)[0] & 1) != 0) return;
        AH_CALL(Kind2_Place)(2);
        AreaW0c_TailState = 0xC;
        return;
    case 0xC:
        if (B(::bof3::Img(at::kScriptVar.v)) != 0x20) return;
        AH_CALL(Flags_Set)(Bank(), 0x5A);
        SetWord(At(::bof3::Img(at::kTailTimer.v)), 0x20);
        AreaW0c_TailState = 0xD;
        return;
    case 0xD: {
        if (!TimerDone()) return;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        AreaW0c_TailSub = slot;
        if (slot == 0xFF) return;
        AreaW0c_TailState = 0xE;
        Kind13(slot, 0xFFFFFD56u, 0x200, 0x20);
        return;
    }
    case 0xE:
        if (B(::bof3::Img(at::kScriptVar.v)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x14:
        AH_CALL(Flags_Toggle)(Bank(), 0x6A);
        AH_CALL(Sound_PlayEffect)(0x204);
        if (AH_CALL(Flags_Test)(Bank(), 0x6A) != 0) AH_CALL(Gfx_ClutAdjust)(0, 0x380, -6, -4, -4);
        else AH_CALL(Gfx_ClutAdjust)(0, 0x180, 0, 0, 0);
        Draw_PassFlags = 0;
        SetWord(At(::bof3::Img(at::kTailTimer.v)), 0xF);
        AreaW0c_TailState = 0x15;
        return;
    case 0x15:
        if (!TimerDone()) return;
        Draw_PassFlags = 0x1F;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    default:
        return;
    }
}

// original 0x422790 (Area_ArriveHook's case for area 148, 0x56E551): z's high
// word 0x1C and x's 0x12..0x14 (16-bit): ScriptFlags_Set40, tail kind 31 at
// state 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area148_ArriveHook(long x, long z) {
    if (High(z) != 0x1C) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x12) >= 3) return 0;
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = 0x1F;
    AreaW0c_TailState = 0;
    return 1;
}

// original 0x4227C0 (Field_ObjectTriggers id 17, 0x662E60): ScriptFlags_Set40;
// tail kind 31 at state 0xA. al 0.
extern "C" unsigned char __cdecl Area148_Trigger17(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = 0x1F;
    AreaW0c_TailState = 0xA;
    return 0;
}

// original 0x4227E0 (Area_CellHooks pair 24, area 0x94): the switch record
// 0x6353F0 (one: x, z, facing in the low nibble, a tail state) whose x and z
// are the argument bytes and facing the leader's +8 (the whole byte): none al
// 0; found: ScriptFlags_Set40, tail kind 31 at the record's state (0x14), al 1.
extern "C" unsigned char __cdecl Area148_SwitchHook(long x, long z) {
    const unsigned char facing = ObjTrio[0].direction;
    const auto bx = static_cast<unsigned char>(x), bz = static_cast<unsigned char>(z);
    U found = 0;
    for (U rec = ::bof3::Img(Area148_Switch_addr); rec < ::bof3::Img(at::kA148SwitchEnd.v); rec += 4, ++found) {
        if (B(rec) == bx && B(rec + 1) == bz && (B(rec + 2) & 0xF) == facing) break;
    }
    if (found == 1) return 0;
    AH_CALL(ScriptFlags_Set40)();
    AreaW0c_TailState = B(::bof3::Img(at::kA148Switch.v) + found * 4 + 3);
    AreaW0a_TailKind = 0x1F;
    return 1;
}

// original 0x422840 (Effect_KindHandlers[0x7E], 0x655548): Area148_BeamStates
// 0x6353F4 by Sprite_Current[1] (3; ours aborts past).
extern "C" void __cdecl Area148_BeamRun(void) {
    StateEntry("Area148", "BeamRun", ::bof3::Img(Area148_BeamStates_addr), at::kA148BeamStateCount, Cur()[1])();
}

// original 0x422860 (beam state 0): +8 = +0xB; the word +0x3E += 0xC0;
// Area148_BeamAhead; +8 = (+8 + 3) & 7; Area148_BeamVelocity; +6 = 2, +9 =
// 0x50, +1 = 1 (Sprite_Current read again for each).
extern "C" void __cdecl Area148_BeamStart(void) {
    Cur()[8] = Cur()[0xB];
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, rv::Current<records::SpriteHead>()->height_whole + 0xC0u);
    AH_CALL(Area148_BeamAhead)();
    Cur()[8] = static_cast<unsigned char>((Cur()[8] + 3) & 7);
    AH_CALL(Area148_BeamVelocity)();
    Cur()[6] = 2;
    Cur()[9] = 0x50;
    Cur()[1] = 1;
}

// original 0x4228B0 (beam state 1): with Field_Request set only the draw.
// Else the point +0x18 / +0x1C moves by +0xC / +0x10, +9 - 1, the draw, the
// member turn; at +9 == 0 the facing +2 (& 7), the velocity again, +9 = 0x50
// and +0xA = Rand() % 45 (idiv): not 0, +9 - 1 and state 2 (a pause).
extern "C" void __cdecl Area148_BeamSweep(void) {
    unsigned char* o = Cur();
    if (Field_Request != 0) {
        AH_CALL(Area148_DrawBeam)(reinterpret_cast<const long*>(o + 0x34), reinterpret_cast<const long*>(o + 0x18));
        return;
    }
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->work_18, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(o)->work_18) + static_cast<U>(rv::As<records::SpriteHead>(o)->step_x)));
    o = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->work_1c, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(o)->work_1c) + static_cast<U>(rv::As<records::SpriteHead>(o)->step_z)));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    o = Cur();
    AH_CALL(Area148_DrawBeam)(reinterpret_cast<const long*>(o + 0x34), reinterpret_cast<const long*>(o + 0x18));
    AH_CALL(Area148_BeamTurnMembers)();
    o = Cur();
    if (o[9] != 0) return;
    o[8] = static_cast<unsigned char>((o[8] + 2) & 7);
    AH_CALL(Area148_BeamVelocity)();
    Cur()[9] = 0x50;
    const std::int32_t r = AH_CALL(Rand)();
    Cur()[0xA] = static_cast<unsigned char>(r % 45);
    o = Cur();
    if (o[0xA] == 0) return;
    o[9] = static_cast<unsigned char>(o[9] - 1);
    Cur()[1] = 2;
}

// original 0x422970 (beam state 2): the draw, the member turn; +0xA - 1, at 0
// +9 + 1 and state 1.
extern "C" void __cdecl Area148_BeamPause(void) {
    unsigned char* o = Cur();
    AH_CALL(Area148_DrawBeam)(reinterpret_cast<const long*>(o + 0x34), reinterpret_cast<const long*>(o + 0x18));
    AH_CALL(Area148_BeamTurnMembers)();
    o = Cur();
    o[0xA] = static_cast<unsigned char>(o[0xA] - 1);
    o = Cur();
    if (o[0xA] != 0) return;
    o[9] = static_cast<unsigned char>(o[9] + 1);
    Cur()[1] = 1;
}

// original 0x4229C0: the point five Field_DirectionSteps ahead of (+0x34,
// +0x38) by the facing +8 (unchecked) to (+0x18, +0x1C); +0x20 = the map's
// elevation there (AreaMap_Elevation's low word, sign-extended) << 16.
extern "C" void __cdecl Area148_BeamAhead(void) {
    unsigned char* o = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->work_18, static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + o[8] * 8u))) * 5u + static_cast<U>(rv::As<records::SpriteHead>(o)->x)));
    o = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->work_1c, static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + 4 + o[8] * 8u))) * 5u + static_cast<U>(rv::As<records::SpriteHead>(o)->z)));
    o = Cur();
    const long h = AH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(o)->work_18, rv::As<records::SpriteHead>(o)->work_1c);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_20, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(h))) << 16));
}

// original 0x422A20: the velocity +0xC / +0x10 = the walk deltas 0x6696DC by
// the facing +8 (unchecked), each << 1.
extern "C" void __cdecl Area148_BeamVelocity(void) {
    unsigned char* o = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_x, static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(MoveCmd_F9Steps_addr) + o[8] * 8u))) << 1));
    o = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_z, static_cast<std::int32_t>(static_cast<U>(Long(At(::bof3::Img(MoveCmd_F9Steps_addr) + 4 + o[8] * 8u))) << 1));
}

// original 0x422A50: with +0x3C set to +0x20 for the two asks (then put back,
// through Sprite_Current read again): two cells about the effect, the offsets
// from Area148 0x635414 by (s8 0x635400[(+9 >> 3) * 2 + k] + (+8 & 6) * 4) *
// 2; Party_MemberAt(x, z, 0) there. A member found, Field_ScriptFlags2 bits 10
// and 12 clear and Field_Request 0: its facing & 7, made odd (+1 & 7 when
// even), stored only when its +1 is 1, then Member_SetState2_8(member, 2).
extern "C" void __cdecl Area148_BeamTurnMembers(void) {
    unsigned char* const o = Cur();
    const unsigned q = static_cast<unsigned char>((o[8] & 6) << 2);
    const unsigned s = static_cast<unsigned char>(o[9] >> 3);
    const std::int32_t saved = rv::As<records::SpriteHead>(o)->height;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->height, rv::As<records::SpriteHead>(o)->work_20);
    for (U k = 0; k < 2; ++k) {
        const std::int32_t index = (static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area148_TurnSteps_addr) + s * 2 + k))) + static_cast<std::int32_t>(q)) * 2;
        const std::int32_t dx = static_cast<signed char>(B(::bof3::Img(at::kA148TurnCells.v) + static_cast<U>(index)));
        const std::int32_t dz = static_cast<signed char>(B(::bof3::Img(at::kA148TurnCells.v) + 1 + static_cast<U>(index)));
        unsigned char* const c = Cur();
        const U z = (static_cast<U>(dz) << 16) + static_cast<U>(rv::As<records::SpriteHead>(c)->z);
        const U x = (static_cast<U>(dx) << 16) + static_cast<U>(rv::As<records::SpriteHead>(c)->x);
        const unsigned char member = AH_CALL(Party_MemberAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (member == 0xFF) continue;
        if ((Field_ScriptFlags2 & 0x1400) != 0) continue;
        if (Field_Request != 0) continue;
        unsigned char* const p = PartyRecord(member);
        unsigned char facing = p[8] & 7;
        if ((facing & 1) == 0) facing = (facing + 1) & 7;
        if (p[1] != 1) continue;
        p[8] = facing;
        AH_CALL(Member_SetState2_8)(member, 2);
    }
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, saved);
}

// original 0x422B50 (from, ahead: the effect's +0x34 and +0x18 points): nothing
// unless Draw_PassFlags and story flag 0x6A. A draw-mode packet (dtd 1); the
// map camera (0x494060); at Gfx_PacketNext a line (semi-transparency off) from
// `from` to `ahead` projected (0x494110 into its two vertices), coloured by +6,
// linked (0x20). The line's screen slope as Math_Ratan2(dy, dx) of the two
// vertices' float differences through _ftol; the screen size of (0x40, 0) at
// each end (0x4941E0), each + (Frame_Counter & 1): r1 at `from`, r2 at `ahead`.
// Area148_DrawBeamFan(ahead's vertex, r2, angle + 0xC00);
// Area148_DrawBeamBand(from's vertex, r1, angle + 0x400, ahead's vertex, r2,
// angle + 0xC00); a draw-mode packet (dtd 0). The vertices are read from the
// line's packet (the pointer taken at the start) and each truncated by _ftol.
extern "C" void __cdecl Area148_DrawBeam(const long* from, const long* ahead) {
    if (Draw_PassFlags == 0) return;
    if (AH_CALL(Flags_Test)(Bank(), 0x6A) == 0) return;
    DrawModeLink(1);
    AH_AT(void (__cdecl*)(), area_w3g::kSetMapCamera.v)();
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetLineF2)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 0);
    AH_AT(ProjectFn, area_w3g::kProjectPoint.v)(from, prim + 8);
    AH_AT(ProjectFn, area_w3g::kProjectPoint.v)(ahead, prim + 0x14);
    prim[4] = BeamColour(0);
    prim[5] = BeamColour(1);
    prim[6] = BeamColour(2);
    LinkHere(0x20);
    const U dx = FtolDiff(prim + 0x14, prim + 8);
    const U dy = FtolDiff(prim + 0x18, prim + 0xC);
    const U angle = static_cast<U>(AH_CALL(Math_Ratan2)(FloatOf(static_cast<std::int32_t>(dy)), FloatOf(static_cast<std::int32_t>(dx))));
    const short offset[2] = {0x40, 0};
    U size = 0;
    AH_AT(ScreenSizeFn, area_w3g::kScreenSize.v)(from, offset, &size);
    const U r1 = (Frame_Counter & 1) + size;
    AH_AT(ScreenSizeFn, area_w3g::kScreenSize.v)(ahead, offset, &size);
    const U r2 = (Frame_Counter & 1) + size;
    AH_CALL(Area148_DrawBeamFan)(static_cast<int>(Ftol(prim + 0x14)), static_cast<int>(Ftol(prim + 0x18)), static_cast<int>(r2),
                                 static_cast<int>(angle + 0xC00));
    AH_CALL(Area148_DrawBeamBand)(static_cast<int>(Ftol(prim + 8)), static_cast<int>(Ftol(prim + 0xC)), static_cast<int>(r1),
                                  static_cast<int>(angle + 0x400), static_cast<int>(Ftol(prim + 0x14)),
                                  static_cast<int>(Ftol(prim + 0x18)), static_cast<int>(r2), static_cast<int>(angle + 0xC00));
    DrawModeLink(0);
}

// original 0x422D90 (x0, y0, r1, a1, x1, y1, r2, a2; every one read as its low
// word, signed): two semi-transparent Gouraud quads, the second built at the
// first's pointer + 0x44 as a copy of the first (after its link). Both have
// (x0, y0) and (x1, y1) in the beam's colour (by +6); the first's two black
// vertices at (x0, y0) + r1 (cos, sin)(a1) >> 12 and (x1, y1) + r2 (cos,
// sin)(a2 + 0x800) >> 12, the second's at a1 + 0x800 and a2 (a1, a2 masked to
// 16 bits before the 0x800); each linked (0x44) at the effect's (+0x18, +0x1C).
extern "C" void __cdecl Area148_DrawBeamBand(int x0, int y0, int r1, int a1, int x1, int y1, int r2, int a2) {
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const std::int32_t sx0 = static_cast<std::int16_t>(x0), sy0 = static_cast<std::int16_t>(y0);
    const std::int32_t sx1 = static_cast<std::int16_t>(x1), sy1 = static_cast<std::int16_t>(y1);
    const std::int32_t sr1 = static_cast<std::int16_t>(r1), sr2 = static_cast<std::int16_t>(r2);
    StoreFloat(prim + 8, sx0);
    StoreFloat(prim + 0xC, sy0);
    StoreFloat(prim + 0x18, sx1);
    StoreFloat(prim + 0x1C, sy1);
    const U b1 = static_cast<U>(a1) & 0xFFFF;
    const U b2 = static_cast<U>(a2) & 0xFFFF;
    StoreFloat(prim + 0x28, Scaled(AH_CALL(Math_Cos)(static_cast<int>(b1)), sr1) + sx0);
    StoreFloat(prim + 0x2C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(b1)), sr1) + sy0);
    StoreFloat(prim + 0x38, Scaled(AH_CALL(Math_Cos)(static_cast<int>(b2 + 0x800)), sr2) + sx1);
    StoreFloat(prim + 0x3C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(b2 + 0x800)), sr2) + sy1);
    prim[4] = BeamColour(0);
    prim[5] = BeamColour(1);
    prim[6] = BeamColour(2);
    prim[0x14] = BeamColour(0);
    prim[0x15] = BeamColour(1);
    prim[0x16] = BeamColour(2);
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    prim[0x34] = prim[0x35] = prim[0x36] = 0;
    LinkHere(0x44);
    unsigned char* const next = prim + 0x44;
    std::memmove(next, prim, 0x44);
    StoreFloat(next + 0x28, Scaled(AH_CALL(Math_Cos)(static_cast<int>(b1 + 0x800)), sr1) + sx0);
    StoreFloat(next + 0x2C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(b1 + 0x800)), sr1) + sy0);
    StoreFloat(next + 0x38, Scaled(AH_CALL(Math_Cos)(static_cast<int>(b2)), sr2) + sx1);
    StoreFloat(next + 0x3C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(b2)), sr2) + sy1);
    LinkHere(0x44);
}

// original 0x422FF0 (x, y, r, a; each read as its low word, signed): eight
// semi-transparent Gouraud triangles, each at Gfx_PacketNext (read again): the
// centre (x, y) in the beam's colour, two black rim vertices at (x, y) + r
// (cos, sin)(a) >> 12 and at a + 0x100, a stepping by 0x100 (half a turn in
// all); each linked (0x34) at the effect's (+0x18, +0x1C).
extern "C" void __cdecl Area148_DrawBeamFan(int x, int y, int r, int a) {
    const std::int32_t sx = static_cast<std::int16_t>(x), sy = static_cast<std::int16_t>(y);
    const std::int32_t sr = static_cast<std::int16_t>(r);
    U angle = static_cast<U>(a);
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const prim = Gfx_PacketNext;
        AH_CALL(Gpu_SetPolyG3)(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        StoreFloat(prim + 8, sx);
        StoreFloat(prim + 0xC, sy);
        const U e0 = angle & 0xFFFF;
        StoreFloat(prim + 0x18, Scaled(AH_CALL(Math_Cos)(static_cast<int>(e0)), sr) + sx);
        StoreFloat(prim + 0x1C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(e0)), sr) + sy);
        angle += 0x100;
        const U e1 = angle & 0xFFFF;
        StoreFloat(prim + 0x28, Scaled(AH_CALL(Math_Cos)(static_cast<int>(e1)), sr) + sx);
        StoreFloat(prim + 0x2C, Scaled(AH_CALL(Math_Sin)(static_cast<int>(e1)), sr) + sy);
        prim[4] = BeamColour(0);
        prim[5] = BeamColour(1);
        prim[6] = BeamColour(2);
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        LinkHere(0x34);
    }
}

void Area148_Inject() {
    BOF3_INJECT(Area146_ClearFlag46);
    BOF3_INJECT(Area146_ToExtraObject0);
    BOF3_INJECT(Area148_MemberMessageA);
    BOF3_INJECT(Area148_MemberMessageB);
    BOF3_INJECT(Area148_ChoiceMessage);
    BOF3_INJECT(Area148_ChoiceSetFlag59);
    BOF3_INJECT(Area148_Tail31);
    BOF3_INJECT(Area148_ArriveHook);
    BOF3_INJECT(Area148_Trigger17);
    BOF3_INJECT(Area148_SwitchHook);
    BOF3_INJECT(Area148_BeamRun);
    BOF3_INJECT(Area148_BeamStart);
    BOF3_INJECT(Area148_BeamSweep);
    BOF3_INJECT(Area148_BeamPause);
    BOF3_INJECT(Area148_BeamAhead);
    BOF3_INJECT(Area148_BeamVelocity);
    BOF3_INJECT(Area148_BeamTurnMembers);
    BOF3_INJECT(Area148_DrawBeam);
    BOF3_INJECT(Area148_DrawBeamBand);
    BOF3_INJECT(Area148_DrawBeamFan);
}

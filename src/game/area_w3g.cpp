// World 3's areas 148..151: the code of the PSX's BIN/WORLD03/AREA148..151.EMI
// compiled into the exe at 0x4223A0..0x4249CF - 56 functions, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR3G; docs/area_w3g.md has the
// areas one section each. Area 147 has no code in the band.
//
// Area 148: two handlers that open a message by the first party member of a
// set, two choices, tail kind 31 (a drop-in, two effect-kind-0x13 flashes, a
// light switch that toggles story flag 0x6A and the CLUT), its arrive hook,
// object trigger 17, a cell hook (the switch), and effect kind 0x7E: a
// searchlight that sweeps ahead, pauses at random, turns the party members in
// its cells, and draws itself (a line, a fan of eight triangles and two
// Gouraud quads, x87 through the CRT's _ftol) while flag 0x6A is set.
// Area 149: handler 1's four states (three camera-distance thresholds, two
// dust effects of kind 0x37 about the kind-2 sprite), a view shift the chapter
// code calls (0x56ABC4), tail kind 33 (the view shifted back, then a change of
// area) and its init. Area 150: three choices, tail kind 58 and its step hook.
// Area 151 is a world map (WorldMap_Records record 9): area 87's code
// instruction for instruction over its own tables but for its place hook,
// which has no cell or name-set search (one name set, message 0x16), and no
// plate start of its own (its plate state 0 is area 152's, 0x424BA0). Its 24
// functions are one body here over a WorldMapTables (area_w3g_callees.h); the
// body is area_w3a.cpp's, copied (one shared body is the rebinding pass's).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6); reads and writes by an unchecked
// index that stay in mapped memory are kept, as the original makes them
// (docs/area_w3g.md, the latent defects). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for the
// callees; the group's own callees are called the same way, so each function
// is fuzzed alone.
#include "game/area_w3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3g_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3g::at;
using U = std::uint32_t;
using area_harness::Handler;
using at::WorldMapTables;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Bank() { return At(at::kStoryFlags); }
unsigned char* Bank904000() { return At(at::kFlags904000); }
unsigned char* PartyRecord(U index) { return At(at::kLeader + index * at::kPartyStride); }
unsigned char* EffectAt(U slot) { return At(at::kEffects + slot * at::kEffectStride); }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }
void SetMessage(unsigned id) { SetWord(At(at::kMessage), id); }
unsigned char& B(U address) { return At(address)[0]; }
void Disarm() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// The tails' u16 frame counter less one: true when it reached 0.
bool TimerDone() {
    const auto t = static_cast<std::uint16_t>(Word(At(at::kTailTimer)) - 1u);
    SetWord(At(at::kTailTimer), t);
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

// A world-map copy's own functions another of them calls directly.
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

// ---- x87 as the originals have it -------------------------------------------

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

}  // namespace

// ===========================================================================
// Area 148 (descriptor 0x635388): its choice array +0x34 starts two dwords
// before its handler array +0x3C (choices 2..7 are handlers 0..5); choices 2..5
// / handlers 0..3 are other bands' (0x42C8A0, Area146_ClearFlag46,
// Area146_ToExtraObject0, 0x425DB0), its init the bare ret 0x437CC0.
// ===========================================================================

namespace {

// Handlers 4 and 5 (choices 6 and 7): the first of four member ids (the outer
// loop) that party record 0 .. Field_MemberCount - 1 has at +0x89 (the inner
// loop, the count read once, unsigned, records past the third unchecked):
// Msg_OpenScript(its message) and Field_Request = 2. None: nothing.
void MessageByMember(U ids, U messages) {
    const unsigned count = Field_MemberCount;
    for (U i = 0; i < 4; ++i) {
        const unsigned char id = B(ids + i);
        for (U j = 0; j < count; ++j) {
            if (B(at::kMemberId + j * at::kPartyStride) != id) continue;
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
    const std::int32_t angle = static_cast<std::int16_t>(Word(At(at::kCameraAngle1)));
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, static_cast<std::int32_t>(x));
    SetLong(e + 0x68, angle);
    SetLong(e + 0x6C, static_cast<std::int32_t>(y));
    e[9] = life;
}

// The beam's colour byte k of entry Sprite_Current[6] (three bytes an entry,
// unchecked; the pointer read again for each byte, as the originals do).
unsigned char BeamColour(unsigned k) { return B(at::kA148BeamColours + Cur()[6] * 3u + k); }

using ProjectFn = void (__cdecl*)(const long*, unsigned char*);
using ScreenSizeFn = void (__cdecl*)(const long*, const short*, void*);

// A draw-mode packet (abr 1 on tpage 0x3C0, dtd as given) at Gfx_PacketNext,
// linked at the running effect's (+0x18, +0x1C) with size 0xC.
void DrawModeLink(int dtd) {
    const unsigned tpage = AH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFFu;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    unsigned char* const o = Cur();
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(o + 0x18)), static_cast<U>(Long(o + 0x1C)), 0, 0xC);
}
// A primitive linked at the running effect's (+0x18, +0x1C).
void LinkHere(unsigned size) {
    unsigned char* const o = Cur();
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(o + 0x18)), static_cast<U>(Long(o + 0x1C)), 0, size);
}
// (trig * r) >> 12, the product 32-bit, the shift arithmetic.
std::int32_t Scaled(int trig, std::int32_t r) {
    return static_cast<std::int32_t>(static_cast<U>(trig) * static_cast<U>(r)) >> 12;
}

}  // namespace

// original 0x4223A0 (handler 4 / choice 6; PSX 0x801F3A20): members 5, 2, 8, 4
// in that order - messages 0xB..0xE (Area148 tables 0x6353D8 / 0x6353DC).
extern "C" void __cdecl Area148_MemberMessageA(void) { MessageByMember(at::kA148MemberIdsA, at::kA148MessagesA); }
// original 0x422430 (handler 5 / choice 7; PSX 0x801F3AE4): members 4, 8, 2, 5
// - messages 0xF..0x12 (0x6353E4 / 0x6353E8).
extern "C" void __cdecl Area148_MemberMessageB(void) { MessageByMember(at::kA148MemberIdsB, at::kA148MessagesB); }

// original 0x4224C0 (choice 0): message 6 when the cursor byte is not 0, else 4.
extern "C" void __cdecl Area148_ChoiceMessage(void) { SetMessage(B(at::kCursor) != 0 ? 6u : 4u); }

// original 0x4224E0 (choice 1): the cursor 0: message 5 (stored before the
// call), Flags_Set(0x904030, 0x59); else no message (0xFFFF).
extern "C" void __cdecl Area148_ChoiceSetFlag59(void) {
    if (B(at::kCursor) != 0) {
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
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(Party_DropIn)(0);
        B(at::kTailState) = 1;
        return;
    case 1:
        if (B(at::kScriptVar) != 0x18) return;
        Disarm();
        AH_CALL(Flags_Set)(Bank(), 0x46);
        AH_CALL(Field_ChangeArea)(0x95, 0x1C0000, 0xC0000, 0x80);
        return;
    case 0xA: {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        B(at::kTailArg) = slot;
        if (slot == 0xFF) return;
        B(at::kTailState) = 0xB;
        Kind13(slot, 0xFFFFFC66u, 0x190, 0x30);
        return;
    }
    case 0xB:
        if ((EffectAt(B(at::kTailArg))[0] & 1) != 0) return;
        AH_CALL(Kind2_Place)(2);
        B(at::kTailState) = 0xC;
        return;
    case 0xC:
        if (B(at::kScriptVar) != 0x20) return;
        AH_CALL(Flags_Set)(Bank(), 0x5A);
        SetWord(At(at::kTailTimer), 0x20);
        B(at::kTailState) = 0xD;
        return;
    case 0xD: {
        if (!TimerDone()) return;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        B(at::kTailArg) = slot;
        if (slot == 0xFF) return;
        B(at::kTailState) = 0xE;
        Kind13(slot, 0xFFFFFD56u, 0x200, 0x20);
        return;
    }
    case 0xE:
        if (B(at::kScriptVar) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x14:
        AH_CALL(Flags_Toggle)(Bank(), 0x6A);
        AH_CALL(Sound_PlayEffect)(0x204);
        if (AH_CALL(Flags_Test)(Bank(), 0x6A) != 0) AH_CALL(Gfx_ClutAdjust)(0, 0x380, -6, -4, -4);
        else AH_CALL(Gfx_ClutAdjust)(0, 0x180, 0, 0, 0);
        Draw_PassFlags = 0;
        SetWord(At(at::kTailTimer), 0xF);
        B(at::kTailState) = 0x15;
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
    B(at::kTailKind) = 0x1F;
    B(at::kTailState) = 0;
    return 1;
}

// original 0x4227C0 (Field_ObjectTriggers id 17, 0x662E60): ScriptFlags_Set40;
// tail kind 31 at state 0xA. al 0.
extern "C" unsigned char __cdecl Area148_Trigger17(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x1F;
    B(at::kTailState) = 0xA;
    return 0;
}

// original 0x4227E0 (Area_CellHooks pair 24, area 0x94): the switch record
// 0x6353F0 (one: x, z, facing in the low nibble, a tail state) whose x and z
// are the argument bytes and facing the leader's +8 (the whole byte): none al
// 0; found: ScriptFlags_Set40, tail kind 31 at the record's state (0x14), al 1.
extern "C" unsigned char __cdecl Area148_SwitchHook(long x, long z) {
    const unsigned char facing = B(at::kLeaderDir);
    const auto bx = static_cast<unsigned char>(x), bz = static_cast<unsigned char>(z);
    U found = 0;
    for (U rec = at::kA148Switch; rec < at::kA148SwitchEnd; rec += 4, ++found) {
        if (B(rec) == bx && B(rec + 1) == bz && (B(rec + 2) & 0xF) == facing) break;
    }
    if (found == 1) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailState) = B(at::kA148Switch + found * 4 + 3);
    B(at::kTailKind) = 0x1F;
    return 1;
}

// original 0x422840 (Effect_KindHandlers[0x7E], 0x655548): Area148_BeamStates
// 0x6353F4 by Sprite_Current[1] (3; ours aborts past).
extern "C" void __cdecl Area148_BeamRun(void) {
    StateEntry("Area148", "BeamRun", at::kA148BeamStates, at::kA148BeamStateCount, Cur()[1])();
}

// original 0x422860 (beam state 0): +8 = +0xB; the word +0x3E += 0xC0;
// Area148_BeamAhead; +8 = (+8 + 3) & 7; Area148_BeamVelocity; +6 = 2, +9 =
// 0x50, +1 = 1 (Sprite_Current read again for each).
extern "C" void __cdecl Area148_BeamStart(void) {
    Cur()[8] = Cur()[0xB];
    SetWord(Cur() + 0x3E, Word(Cur() + 0x3E) + 0xC0u);
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
    SetLong(o + 0x18, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x18)) + static_cast<U>(Long(o + 0xC))));
    o = Cur();
    SetLong(o + 0x1C, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x1C)) + static_cast<U>(Long(o + 0x10))));
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
    SetLong(o + 0x18, static_cast<std::int32_t>(static_cast<U>(Long(At(at::kDirSteps + o[8] * 8u))) * 5u + static_cast<U>(Long(o + 0x34))));
    o = Cur();
    SetLong(o + 0x1C, static_cast<std::int32_t>(static_cast<U>(Long(At(at::kDirSteps + 4 + o[8] * 8u))) * 5u + static_cast<U>(Long(o + 0x38))));
    o = Cur();
    const long h = AH_CALL(AreaMap_Elevation)(Long(o + 0x18), Long(o + 0x1C));
    SetLong(Cur() + 0x20, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(h))) << 16));
}

// original 0x422A20: the velocity +0xC / +0x10 = the walk deltas 0x6696DC by
// the facing +8 (unchecked), each << 1.
extern "C" void __cdecl Area148_BeamVelocity(void) {
    unsigned char* o = Cur();
    SetLong(o + 0xC, static_cast<std::int32_t>(static_cast<U>(Long(At(at::kWalkDelta + o[8] * 8u))) << 1));
    o = Cur();
    SetLong(o + 0x10, static_cast<std::int32_t>(static_cast<U>(Long(At(at::kWalkDelta + 4 + o[8] * 8u))) << 1));
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
    const std::int32_t saved = Long(o + 0x3C);
    SetLong(o + 0x3C, Long(o + 0x20));
    for (U k = 0; k < 2; ++k) {
        const std::int32_t index = (static_cast<std::int32_t>(static_cast<signed char>(B(at::kA148TurnSteps + s * 2 + k))) + static_cast<std::int32_t>(q)) * 2;
        const std::int32_t dx = static_cast<signed char>(B(at::kA148TurnCells + static_cast<U>(index)));
        const std::int32_t dz = static_cast<signed char>(B(at::kA148TurnCells + 1 + static_cast<U>(index)));
        unsigned char* const c = Cur();
        const U z = (static_cast<U>(dz) << 16) + static_cast<U>(Long(c + 0x38));
        const U x = (static_cast<U>(dx) << 16) + static_cast<U>(Long(c + 0x34));
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
    SetLong(Cur() + 0x3C, saved);
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
    AH_AT(void (__cdecl*)(), area_w3g::kSetMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetLineF2)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 0);
    AH_AT(ProjectFn, area_w3g::kProjectPoint)(from, prim + 8);
    AH_AT(ProjectFn, area_w3g::kProjectPoint)(ahead, prim + 0x14);
    prim[4] = BeamColour(0);
    prim[5] = BeamColour(1);
    prim[6] = BeamColour(2);
    LinkHere(0x20);
    const U dx = FtolDiff(prim + 0x14, prim + 8);
    const U dy = FtolDiff(prim + 0x18, prim + 0xC);
    const U angle = static_cast<U>(AH_CALL(Math_Ratan2)(FloatOf(static_cast<std::int32_t>(dy)), FloatOf(static_cast<std::int32_t>(dx))));
    const short offset[2] = {0x40, 0};
    U size = 0;
    AH_AT(ScreenSizeFn, area_w3g::kScreenSize)(from, offset, &size);
    const U r1 = (Frame_Counter & 1) + size;
    AH_AT(ScreenSizeFn, area_w3g::kScreenSize)(ahead, offset, &size);
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

// ===========================================================================
// Area 149 (descriptor 0x6361B8; handler 0 is Area146_ToExtraObject0)
// ===========================================================================

namespace {

// The dust states' shared tail: Area149_SpawnDust(dx), then the running
// script's word +0xA - 2 (MoveScript_Object's: the op runs again).
void DustThenAgain(int dx) {
    AH_CALL(Area149_SpawnDust)(static_cast<signed char>(dx));
    unsigned char* const script = MoveScript_Object;
    SetWord(script + 0xA, Word(script + 0xA) + 0xFFFEu);
}
// Extra record 0's x (s32 +0x34) below the kind-2 x + 0x20000 (signed): its
// +0x84 = 3.
void MarkIfBehind() {
    if (Long(At(at::kExtra0 + 0x34)) < static_cast<std::int32_t>(static_cast<U>(Field_Kind2X) + 0x20000u))
        B(at::kExtra0 + 0x84) = 3;
}
// The map moved 0x14 cells in x (`sign` +1 or -1): Sprite_Kind2 +0x34 and
// Field_Kind2X by 0x140000, MapView_Origin's word by 0x14, MapView_FocusX to
// `focus`; the kind-2 sprite's height +0x3E from AreaMap_Elevation there,
// MapView_SetElevation(it), the view shift 0x56FCA0; with `effects` every
// live (bit 0) dust record (kind 0x37) of the twenty moved and its +0x3E
// again; extra record 0's x moved and its +0x3E again.
void ShiftMap(int sign, U focus, bool effects) {
    const U step = sign > 0 ? 0x140000u : 0xFFEC0000u;
    const U kind2x = static_cast<U>(Field_Kind2X);
    SetWord(At(at::kOrigin), Word(At(at::kOrigin)) + (sign > 0 ? 0x14u : 0xFFECu));
    SetLong(At(at::kKind2X), static_cast<std::int32_t>(static_cast<U>(Long(At(at::kKind2X))) + step));
    const std::int32_t x = static_cast<std::int32_t>(kind2x + step);
    const std::int32_t z = Field_Kind2Z;
    SetLong(At(at::kFocusX), static_cast<std::int32_t>(focus));
    Field_Kind2X = x;
    const long h = AH_CALL(AreaMap_Elevation)(x, z);
    SetWord(At(at::kKind2Height), static_cast<U>(h));
    AH_CALL(MapView_SetElevation)(static_cast<int>(h));
    AH_AT(void (__cdecl*)(), area_w3g::kViewShift)();
    if (effects) {
        for (U k = 0; k < at::kEffectCount; ++k) {
            unsigned char* const e = EffectAt(k);
            if ((e[0] & 1) == 0 || e[5] != 0x37) continue;
            SetLong(e + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(e + 0x34)) + step));
            const long eh = AH_CALL(AreaMap_Elevation)(Long(e + 0x34), Long(e + 0x38));
            SetWord(e + 0x3E, static_cast<U>(eh));
        }
    }
    const std::int32_t ex = static_cast<std::int32_t>(static_cast<U>(Long(At(at::kExtra0 + 0x34))) + step);
    SetLong(At(at::kExtra0 + 0x34), ex);
    const long xh = AH_CALL(AreaMap_Elevation)(ex, Long(At(at::kExtra0 + 0x38)));
    SetWord(At(at::kExtra0 + 0x3E), static_cast<U>(xh));
}

}  // namespace

// original 0x423160 (handler 1; PSX 0x801F2EFC): Area149_DustStates 0x6361FC
// by Sprite_Current[4] (4; ours aborts past).
extern "C" void __cdecl Area149_DustRun(void) {
    StateEntry("Area149", "DustRun", at::kA149DustStates, at::kA149DustStateCount, Cur()[4])();
}

// original 0x423180 (dust state 0): Camera_Distance (s16) below 0x880: extra
// record 0's +0x84 = 1, counter 0 = 0xA, Sound_PlayEffect(1), +4 + 1. Then
// the dust at -4 and the op again.
extern "C" void __cdecl Area149_DustNear880(void) {
    if (Camera_Distance < 0x880) {
        B(at::kExtra0 + 0x84) = 1;
        B(at::kCounter0) = 0xA;
        AH_CALL(Sound_PlayEffect)(1);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    DustThenAgain(-4);
}

// original 0x4231D0 (dust state 1): below 0x680: counter 0 = 0xF,
// Sound_PlayEffect(0), +4 + 1. Then extra record 0 behind the kind-2 x + 2
// cells: its +0x84 = 3; the dust at -3 and the op again.
extern "C" void __cdecl Area149_DustNear680(void) {
    if (Camera_Distance < 0x680) {
        B(at::kCounter0) = 0xF;
        AH_CALL(Sound_PlayEffect)(0);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    MarkIfBehind();
    DustThenAgain(-3);
}

// original 0x423230 (dust state 2): Camera_Distance 0: counter 0 = 0x14,
// Sound_PlayEffect(1), +4 + 1. Then as state 1, the dust at -2.
extern "C" void __cdecl Area149_DustAtZero(void) {
    if (Camera_Distance == 0) {
        B(at::kCounter0) = 0x14;
        AH_CALL(Sound_PlayEffect)(1);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    MarkIfBehind();
    DustThenAgain(-2);
}

// original 0x423290 (dust state 3): the dust at -1 and the op again.
extern "C" void __cdecl Area149_DustHold(void) { DustThenAgain(-1); }

// original 0x4232B0 (dx, a signed byte): nothing on frames with Frame_Counter
// & 3. Else twice (z one cell ahead, then one behind): an Effect_FindFree slot
// (none: the next) of kind 0x37 at (Field_Kind2X + dx << 16, Field_Kind2Z +-
// 0x10000), the word +0x2C 0x2C0 (twice), +0x3E the elevation there, +9 1, +6
// and +7 0, +0x29 6.
extern "C" void __cdecl Area149_SpawnDust(signed char dx) {
    if ((Frame_Counter & 3) != 0) return;
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot == 0xFF) continue;
        unsigned char* const e = EffectAt(slot);
        const U x = (static_cast<U>(static_cast<std::int32_t>(dx)) << 16) + static_cast<U>(Field_Kind2X);
        const U z = (k != 0 ? 0xFFFF0000u : 0x10000u) + static_cast<U>(Field_Kind2Z);
        e[0] = 1;
        e[5] = 0x37;
        SetWord(e + 0x2C, 0x2C0);
        SetLong(e + 0x34, static_cast<std::int32_t>(x));
        SetLong(e + 0x38, static_cast<std::int32_t>(z));
        const long h = AH_CALL(AreaMap_Elevation)(Long(e + 0x34), static_cast<long>(z));
        SetWord(e + 0x3E, static_cast<U>(h));
        SetWord(e + 0x2C, 0x2C0);
        e[9] = 1;
        e[6] = 0;
        e[7] = 0;
        e[0x29] = 6;
    }
}

// original 0x423380 (called by Scena15_Runs' step 0x33, 0x56ABC4): with
// MapView_FocusX at 0x63FF the map moved 0x14 cells back in x (focus 0x77FF),
// the dust records with it.
extern "C" void __cdecl Area149_ViewShiftBack(void) {
    if (static_cast<U>(Long(At(at::kFocusX))) != 0x63FF) return;
    ShiftMap(-1, 0x77FF, true);
}

// original 0x423450 (Field_ModeTailKinds[33]): by the s8 0x9039F4 through the
// byte table 0x423610 and the case table 0x4235FC (states 0..0xC):
//   0: with MapView_FocusX at 0x77FF the map moved 0x14 cells on (focus
//      0x63FF; no dust records); the timer - 1, at 0 state 2.
//   2: the script byte 0x90384B = 0x20, the tail disarmed,
//      Field_ChangeArea(0xA7, 0x280000, 0x630000, 0x81).
//   0xA: with the focus at 0x63FF the map moved back (focus 0x77FF); the
//      timer - 1, at 0 state 0xC.
//   0xC: as 2 to area 0x94 (0xA0000, 0x190000, 0x81).
extern "C" void __cdecl Area149_Tail33(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        if (static_cast<U>(Long(At(at::kFocusX))) == 0x77FF) ShiftMap(1, 0x63FF, false);
        if (TimerDone()) B(at::kTailState) = 2;
        return;
    case 2:
        B(at::kScriptVar) = 0x20;
        Disarm();
        AH_CALL(Field_ChangeArea)(0xA7, 0x280000, 0x630000, 0x81);
        return;
    case 0xA:
        if (static_cast<U>(Long(At(at::kFocusX))) == 0x63FF) ShiftMap(-1, 0x77FF, false);
        if (TimerDone()) B(at::kTailState) = 0xC;
        return;
    case 0xC:
        B(at::kScriptVar) = 0x20;
        Disarm();
        AH_CALL(Field_ChangeArea)(0x94, 0xA0000, 0x190000, 0x81);
        return;
    default:
        return;
    }
}

// original 0x423620 (area 149's init; PSX 0x801F36CC): come from area 0x94:
// extra record 0's x high word + 0x16, +0x83 = 1, the word +0x8A = 0, +0x3E
// its elevation, Cond_ByteFE = 1; with flag 0x11 of the bank 0x904000 tail
// kind 33 at state 0 and the timer 0x5A. Come from area 0xA7: +0x8A = 0, +0x83
// and Cond_ByteFE 2, tail kind 33 at state 0xA, the timer 0x5A.
extern "C" void __cdecl Area149_Init(void) {
    unsigned char* const x0 = At(at::kExtra0);
    if (Word(At(at::kCameFrom)) == 0x94) {
        SetWord(x0 + 0x36, Word(x0 + 0x36) + 0x16u);
        const std::int32_t z = Long(x0 + 0x38), x = Long(x0 + 0x34);
        x0[0x83] = 1;
        SetWord(x0 + 0x8A, 0);
        const long h = AH_CALL(AreaMap_Elevation)(x, z);
        SetWord(x0 + 0x3E, static_cast<U>(h));
        Cond_ByteFE = 1;
        if (AH_CALL(Flags_Test)(Bank904000(), 0x11) != 0) {
            B(at::kTailKind) = 0x21;
            B(at::kTailState) = 0;
            SetWord(At(at::kTailTimer), 0x5A);
        }
    }
    if (Word(At(at::kCameFrom)) == 0xA7) {
        SetWord(x0 + 0x8A, 0);
        x0[0x83] = 2;
        Cond_ByteFE = 2;
        B(at::kTailKind) = 0x21;
        B(at::kTailState) = 0xA;
        SetWord(At(at::kTailTimer), 0x5A);
    }
}

// ===========================================================================
// Area 150 (descriptor 0x636A18: a choice table only, no handlers, no init)
// ===========================================================================

// original 0x4236D0 (choice 0): the message from 0x636A5C by the s8 cursor
// (unchecked). Cursor 0: item 0x5B topped up to 16 - Inventory_Add(0, 0x5B,
// 16 - Inventory_Count(0, 0x5B, 0) as a byte, 0) - and Sound_PlayEffect(0x106)
// when it answers (al) not 0.
extern "C" void __cdecl Area150_ChoiceFill5B(void) {
    const auto cursor = static_cast<signed char>(B(at::kCursor));
    SetMessage(Word(At(at::kA150MessagesA + static_cast<U>(static_cast<std::int32_t>(cursor) * 2))));
    if (cursor != 0) return;
    const unsigned short have = AH_CALL(Inventory_Count)(0, 0x5B, 0);
    const unsigned count = static_cast<unsigned char>(0x10 - static_cast<unsigned char>(have));
    // (the original pushes a fourth word, 0, that Inventory_Add does not read)
    if (AH_CALL(Inventory_Add)(0, 0x5B, count) != 0) AH_CALL(Sound_PlayEffect)(0x106);
}

// original 0x423720 (choice 1): the message from 0x636A60 by the s8 cursor.
extern "C" void __cdecl Area150_ChoiceMessage(void) {
    const auto cursor = static_cast<signed char>(B(at::kCursor));
    SetMessage(Word(At(at::kA150MessagesB + static_cast<U>(static_cast<std::int32_t>(cursor) * 2))));
}

// original 0x423740 (choice 2): no message; the tail's state 0xE when the
// cursor byte is not 0, else 0xC.
extern "C" void __cdecl Area150_ChoiceTailState(void) {
    const unsigned char cursor = B(at::kCursor);
    SetMessage(0xFFFF);
    B(at::kTailState) = cursor != 0 ? 0xE : 0xC;
}

// original 0x423760 (Field_ModeTailKinds[58]): by the s8 0x9039F4 - 0xA
// through the byte table 0x4238FC and the case table 0x4238E0 (states
// 0xA..0x16):
//   0xA: Msg_OpenScript(0x2D), Field_Request 2, state 0xB (the choice then
//        sets 0xC or 0xE).
//   0xC: once Field_Request is not 2: ScriptFlags_Clear40,
//        Field_ChangeArea(0xBD, 0x17800000, 0xF000000, 0xA);
//        Field_ScriptFlags2 bit 6; 0x904153 = 0xA, the word 0x90405C = 0,
//        0x90405F = 0xF0, 0x90405E, 0x929EC1, 0x9036D0 = 0; Field_StatusBits
//        bit 0; the tail disarmed.
//   0xE: once Field_Request is not 2: ScriptFlags_Clear40, disarmed.
//   0x14: with Field_Request 0: Msg_OpenScript(0x25 with flag 3 of the bank
//        0x904000, else 0x20), Field_Request 2, state 0x15.
//   0x15: once Field_Request is not 2: Party_HealJoined, Sound_LoadStream(0),
//        state 0x16.
//   0x16: Sound_StreamDone (all of eax) not 0: counter 0 = 1,
//        Music_Play(0x95, 0x10), Transition_Start(1), Draw_PassFlags 0x1F,
//        flag 3 of 0x904000 and flag 0x8A of 0x904030 cleared, disarmed.
extern "C" void __cdecl Area150_Tail58(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0xA:
        AH_CALL(Msg_OpenScript)(0x2D);
        Field_Request = 2;
        B(at::kTailState) = 0xB;
        return;
    case 0xC:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0xBD, 0x17800000, 0xF000000, 0xA);
        {
            const unsigned char status = Field_StatusBits;
            Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x40);   // `or byte`, its low byte
            B(at::kHealByte) = 0xA;
            SetWord(At(at::kWord90405C), 0);
            B(at::kByte90405F) = 0xF0;
            B(at::kByte90405E) = 0;
            B(at::kByte929EC1) = 0;
            B(at::kByte9036D0) = 0;
            Field_StatusBits = static_cast<unsigned char>(status | 1);
        }
        Disarm();
        return;
    case 0xE:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x14:
        if (Field_Request != 0) return;
        AH_CALL(Msg_OpenScript)(AH_CALL(Flags_Test)(Bank904000(), 3) != 0 ? 0x25 : 0x20);
        Field_Request = 2;
        B(at::kTailState) = 0x15;
        return;
    case 0x15:
        if (Field_Request == 2) return;
        AH_CALL(Party_HealJoined)();
        AH_CALL(Sound_LoadStream)(0);
        B(at::kTailState) = 0x16;
        return;
    case 0x16:
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        B(at::kCounter0) = 1;
        AH_CALL(Music_Play)(0x95, 0x10);
        AH_CALL(Transition_Start)(1);
        Draw_PassFlags = 0x1F;
        AH_CALL(Flags_Clear)(Bank904000(), 3);
        AH_CALL(Flags_Clear)(Bank(), 0x8A);
        Disarm();
        return;
    default:
        return;
    }
}

// original 0x423910 (Area_StepHook's case for area 150, 0x56E1B2): x's high
// word 0x17..0x1C and z's 0x33..0x38 (16-bit): ScriptFlags_Set40, tail kind 58
// at state 0xA, al 1. Else al 0.
extern "C" unsigned char __cdecl Area150_StepHook(long x, long z) {
    if (static_cast<std::uint16_t>(High(x) - 0x17) >= 6) return 0;
    if (static_cast<std::uint16_t>(High(z) - 0x33) >= 6) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x3A;
    B(at::kTailState) = 0xA;
    return 1;
}

// ===========================================================================
// The world-map body (area 151's; area_w3a.cpp's copy for area 115, itself
// area_w2b.cpp's for areas 87 and 88, area 45's code, docs/area_w1b.md
// section 5; area 16's in docs/area_w0b.md; area 33's in docs/worldmap_area.md
// sections 1..4)
// ===========================================================================

namespace {

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (At(at::kMapMode)[0] != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

// The first of `count` button-table entries whose mask word has a bit of the
// button word's low 16 bits: its sprite byte, or -1.
int KeyIndex(const WorldMapTables& t, U button_word, U count) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* const entry = At(t.buttons + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
}

// Area 151's field hook (WorldMap_FieldHooks[9]; 0x56DE30 calls the entry of
// WorldMap_RecordIndex): area 87's two-state machine on the s8 0x9039F4 with
// the cell and name-set searches gone.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first place row whose word is the place 0x937F82, the row
//      count when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript. Any other:
//      up to five Text_Records rows from the one name set's items (0xFF ends
//      them): "????????" and a 0 for an item whose byte at 0x9040EC is 0, else
//      the item's 16-byte name (0x669CD8 for item 0x16, else Item_NamePtr(0,
//      item + 0x38)); Msg_OpenScript(0x16). Then 0x9039F4 (read again) + 1 and
//      Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: the row is not checked.
void PlaceMessage151(const WorldMapTables& t) {
    const auto state = static_cast<signed char>(At(at::kMsgState)[0]);
    if (state == 1) {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        At(at::kMsgMode)[0] = 0;
        At(at::kMsgState)[0] = 0;
        At(at::kMsgArg)[0] = 0;
        return;
    }
    if (state != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    const auto z = static_cast<short>(Word(At(at::kLeaderCellWordZ)));
    const auto x = static_cast<short>(Word(At(at::kLeaderCellWordX)));
    if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA1) {
        const unsigned place = Word(At(at::kPlace));
        std::int32_t row = 0;
        for (U a = t.place_messages; a < t.place_messages_end; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(t.place_messages + static_cast<U>(index * 2)))));
    } else {
        const unsigned char* items = At(t.name_items);
        for (U row = at::kTextRecords; row < t.text_rows_end; row += 0x20, ++items) {
            const unsigned item = items[0];
            if (item == 0xFF) break;
            unsigned char* const out = At(row);
            if (At(at::kItemsHeld + item)[0] == 0) {
                SetLong(out, 0x3F3F3F3F);
                SetLong(out + 4, 0x3F3F3F3F);
                out[8] = 0;
            } else {
                const unsigned char* const name =
                    item == 0x16 ? At(at::kItemNameKey) : AH_CALL(Item_NamePtr)(0, (item + 0x38) & 0xFF);
                std::memcpy(out, name, 16);
            }
        }
        AH_CALL(Msg_OpenScript)(0x16);
    }
    const auto next = static_cast<unsigned char>(At(at::kMsgState)[0] + 1);
    Field_Request = 2;
    At(at::kMsgState)[0] = next;
}

// The place plate (the record's +0: effect kind 0). The cell ahead of the
// leader - the high words of (leader +9) * (+0xC) + (+0x34) and of the same
// with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the kind +0xB by
// AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2, 0xAE 3, else
// Field_ScriptFlags2 bit 12 4, else 0; then the plate state table by +1
// (read after the calls; ours aborts past its five entries).
void PlateRun(const WorldMapTables& t) {
    const U steps = At(at::kLeaderSteps)[0];
    const U zs = steps * static_cast<U>(Long(At(at::kLeaderDirZ))) + static_cast<U>(Long(At(at::kLeaderZ)));
    const U xs = steps * static_cast<U>(Long(At(at::kLeaderDirX))) + static_cast<U>(Long(At(at::kLeaderX)));
    const auto x = static_cast<short>(xs >> 16);
    const auto z = static_cast<short>(zs >> 16);
    SetLong(Cur() + 0xC, x);
    SetLong(Cur() + 0x10, z);
    unsigned char kind;
    if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA1) kind = 1;
    else if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA0) kind = 2;
    else if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xAE) kind = 3;
    else if ((Field_ScriptFlags2 & 0x1000) != 0) kind = 4;
    else kind = 0;
    Cur()[0xB] = kind;
    StateEntry(t.name, "PlateRun", t.plate_states, 5, Cur()[1])();
}

// The plate's state 1: +7 = +0xB. Kind 1: the plate animation entry whose
// word is the place 0x937F82 - searched with NO bound - +0x18 = the place
// (zero-extended), its animation byte. Kinds 2, 3, 4: animations 3, 0, 1.
// Each: +0x40 = 0, +0x44 = 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation,
// +1 = 2. Other kinds: no more.
void PlateShow(const WorldMapTables& t) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = t.plate_anims;
        while (Word(At(entry)) != place) entry += 4;
        SetLong(Cur() + 0x18, static_cast<std::int32_t>(place));
        animation = At(entry + 2)[0];
    } else if (kind == 2) {
        animation = 3;
    } else if (kind == 3) {
        animation = 0;
    } else if (kind == 4) {
        animation = 1;
    } else {
        return;
    }
    SetLong(Cur() + 0x40, 0);
    SetLong(Cur() + 0x44, 0x10000);
    Cur()[0x48] = 2;
    Cur()[9] = 8;
    AH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Cur()[1] = 2;
}

// The plate's state 2: WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0
// +0x48 = 0 and +1 = 3; then a tail jump to Sprite_QueueOverlay.
void PlateGrow() {
    AH_CALL(WorldMap_PinSprite)();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) + 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] == 0) {
        o[0x48] = 0;
        Cur()[1] = 3;
    }
    AH_CALL(Sprite_QueueOverlay)();
}

// The plate's state 3: WorldMap_PinSprite; unless Game_Mode is 1 the plate
// leaves (+0x48 = 2, +9 = 8, +1 = 4) when +0xB is not +7, or Field_Request
// is 5, or +7 is 1 and +0x18 is not the place (zero-extended); a tail jump to
// Sprite_QueueOverlay.
void PlateHold() {
    AH_CALL(WorldMap_PinSprite)();
    if (Game_Mode != 1) {
        unsigned char* const o = Cur();
        const unsigned shown = o[7];
        const bool leaves = o[0xB] != shown || Field_Request == 5 ||
                            (shown == 1 && static_cast<U>(Long(o + 0x18)) != Word(At(at::kPlace)));
        if (leaves) {
            o[0x48] = 2;
            Cur()[9] = 8;
            Cur()[1] = 4;
        }
    }
    AH_CALL(Sprite_QueueOverlay)();
}

// The plate's state 4: WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0:
// a tail jump to Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to
// Effect_Release, else +0x48 = 0 and +1 = 1.
void PlateShrink() {
    AH_CALL(WorldMap_PinSprite)();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) - 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] != 0) {
        AH_CALL(Sprite_QueueOverlay)();
        return;
    }
    if (Field_Request == 5) {
        AH_CALL(Effect_Release)();
        return;
    }
    o[0x48] = 0;
    Cur()[1] = 1;
}

// The dial frame's slide, state 1: the word +0x2E += 0x10; at 0x10 and above
// (s16) +2 + 1; then a tail jump to the copy's FrameHold.
void FrameSlideIn(const WorldMapTables& t) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + 0x10u);
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(t.fn_frame_hold)();
}

// State 2: the mode byte 2 makes +2 = 3; the copy's DrawFrame(0x10, the word
// +0x2E). The y pushed carries a stale high half the drawing never reads:
// ours passes the word sign-extended.
void FrameHold(const WorldMapTables& t) {
    if (At(at::kMapMode)[0] == 2) Cur()[2] = 3;
    AH_AT(DrawAt, t.fn_draw_frame)(0x10, S16(Cur() + 0x2E));
}

// State 3: the word +0x2E -= 0x10; at -0x30 and below +2 = 0; unless the
// mode byte is 2, +2 = 1 (over the 0); DrawFrame(0x10, y).
void FrameSlideOut(const WorldMapTables& t) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) - 0x10u);
    unsigned char* o = Cur();
    if (S16(o + 0x2E) <= -0x30) {
        o[2] = 0;
        o = Cur();
    }
    if (At(at::kMapMode)[0] != 2) {
        o[2] = 1;
        o = Cur();
    }
    AH_AT(DrawAt, t.fn_draw_frame)(0x10, S16(o + 0x2E));
}

// The region box, state 1: y +0x30 -= 10; at 0xC8 and below (s16) +3 + 1;
// when the box leaves +3 = 3; the copy's DrawHud(0x5C, y) - the y pushed
// with the object pointer's high half above the word, which the drawing
// never reads.
void BoxSlideIn(const WorldMapTables& t) {
    SetWord(Cur() + 0x30, Word(Cur() + 0x30) - 10u);
    unsigned char* o = Cur();
    if (S16(o + 0x30) <= 0xC8) {
        o[3] = static_cast<unsigned char>(o[3] + 1);
        o = Cur();
    }
    if (BoxLeaves(o)) {
        o[3] = 3;
        o = Cur();
    }
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// State 2: while +0xB is 0 +9 + 1, at 0x5A and above (u8) +0xB + 1; when the
// box leaves +3 + 1; DrawHud(0x5C, y).
void BoxHold(const WorldMapTables& t) {
    unsigned char* o = Cur();
    if (o[0xB] == 0) {
        o[9] = static_cast<unsigned char>(o[9] + 1);
        o = Cur();
        if (o[9] >= 0x5A) {
            o[0xB] = static_cast<unsigned char>(o[0xB] + 1);
            o = Cur();
        }
    }
    if (BoxLeaves(o)) {
        o[3] = static_cast<unsigned char>(o[3] + 1);
        o = Cur();
    }
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// State 3: y +0x30 += 10; at 0xF0 and above (s16) +3 = 0; unless the mode
// byte is set (whatever +0xB), Field_Request is 2 or Field_ScriptFlags bit
// 8, +3 = 1; DrawHud(0x5C, y).
void BoxSlideOut(const WorldMapTables& t) {
    SetWord(Cur() + 0x30, Word(Cur() + 0x30) + 10u);
    unsigned char* o = Cur();
    if (S16(o + 0x30) >= 0xF0) {
        o[3] = 0;
        o = Cur();
    }
    if (At(at::kMapMode)[0] == 0 && Field_Request != 2 && (Field_ScriptFlags & 0x100) == 0) {
        o[3] = 1;
        o = Cur();
    }
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// The dial frame at (x, y) (area 33's WorldMap_DrawFrame 0x404390,
// instruction for instruction), nothing unless Draw_PassFlags & 0x1B. A
// draw-mode primitive (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to
// slot 1; the dial (sprite 0) at (x, y); the cell under the leader
// (AreaMap_ByteAt of the high words of Field_Kind2X / Z); legend 1 at (x +
// 0x30, y) unless the cell is 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit
// 12, its key (the first of six button entries whose mask has a bit of the
// button word 0) at (x + 0x38, y + 8) as the entry's sprite + 1 (lit) or the
// sprite (withheld); legend 2 at (x + 0x30, y + 0x10) unless the cell is 0xA0
// / 0xA1, its key from word 6 over EIGHT entries (the seventh and eighth are
// the record-8 state table's words) at (x + 0x38, y + 0x10); legend 3 at (x +
// 0x30, y + 0x18) when Field_CellHasEvent(cell x, z) or flag bit 12, or the
// party set is 0xC, or Field_ScriptFlags bit 14; the needle at (x + 0x18, y +
// 0x18).
void DrawFrame(const WorldMapTables& t, int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    const auto sprite = AH_AT(DrawSpriteAt, t.fn_draw_sprite);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    sprite(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                                       static_cast<short>(Word(At(at::kLeaderCellZ))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) sprite(x + 0x30, y, 1);
    {
        const int key = KeyIndex(t, static_cast<U>(Long(At(at::kButtonMap0))), 6);
        if (key >= 0) sprite(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) sprite(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(t, static_cast<U>(Long(At(at::kButtonMap6))), 8);
        if (key >= 0) sprite(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                            static_cast<short>(Word(At(at::kLeaderCellZ)))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (At(at::kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) sprite(x + 0x30, y + 0x18, 3);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// One sprite of the dial page at (x, y) (area 33's WorldMap_DrawSprite
// 0x404560): a draw-mode primitive committed to slot 1, then at
// Gfx_PacketNext (read again) a SPRT, semi-transparent when the index's low
// byte is not 0, colour 0x80 x 3, x and y as floats of the arguments' low
// words, CLUT 0x7B80, (w, h, u, v) from the sprite table by index & 0xFF
// (unchecked); Gfx_CommitPrim(1, 0x1C).
void DrawSprite(const WorldMapTables& t, int x, int y, unsigned index) {
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetSprt)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, (index & 0xFF) != 0 ? 1u : 0u);
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, 4);
    prim[4] = prim[5] = prim[6] = 0x80;
    std::memcpy(prim + 0xC, &fy, 4);
    SetWord(prim + 0x16, 0x7B80);
    const unsigned char* const entry = At(t.sprites + (index & 0xFF) * 4u);
    SetWord(prim + 0x18, entry[0]);
    SetWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// The region box (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the box (sprite 4) at (x, y), its cap (5) at (x +
// 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low word
// of the copy's label dword, read after the sprites - 0x803580 itself for
// areas 87, 88, 121 and 151, 0x803584 for area 45, 0x803588 for area 16).
void DrawHud(const WorldMapTables& t, int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x, y, 4);
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(t.label_offset))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// The record's +8 state 1 (area 33's copy 0x4046A0):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of the direction table's
// entry d; +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words
// +0x36 / +0x3A less each direction word >> 13 (arithmetic, 16-bit), then
// +0x36 (when +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down
// otherwise. Then the words +0x36 / +0x3A less the direction words >> 9;
// +0xB = 0, +1 + 1; Sprite_SetAnimation(the record-8 animation entry d's
// byte 0), +0x2A = its byte 1 (d read again after the call); a tail jump to
// Sprite_UpdateScreen.
void Record8Place(const WorldMapTables& t) {
    const auto dir = [&t](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(t.directions + half + o[8] * 4u))); };
    AH_CALL(Sprite_SetAnimationBank)(0x46);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x29] = 5;
    SetLong(Cur() + 0xC, dir(Cur(), 0));
    SetLong(Cur() + 0x10, dir(Cur(), 2));
    SetLong(Cur() + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    unsigned char* o = Cur();
    if (o[6] != 0) {
        SetWord(o + 0x36, Word(o + 0x36) - static_cast<unsigned>(dir(o, 0) >> 13));
        o = Cur();
        SetWord(o + 0x3A, Word(o + 0x3A) - static_cast<unsigned>(dir(o, 2) >> 13));
        o = Cur();
        const bool along_x = Long(o + 0xC) == 0;
        const unsigned step = o[6] == 1 ? 2u : 0xFFFEu;
        if (along_x) SetWord(o + 0x36, Word(o + 0x36) + step);
        else SetWord(o + 0x3A, Word(o + 0x3A) + step);
        o = Cur();
    }
    SetWord(o + 0x36, Word(o + 0x36) - static_cast<unsigned>(dir(o, 0) >> 9));
    o = Cur();
    SetWord(o + 0x3A, Word(o + 0x3A) - static_cast<unsigned>(dir(o, 2) >> 9));
    Cur()[0xB] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    AH_CALL(Sprite_SetAnimation)(At(t.record8_anims + Cur()[8] * 2u)[0]);
    o = Cur();
    o[0x2A] = At(t.record8_anims + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// The record's +4 state 0 (area 33's copy 0x404820): with the byte 0x903A79
// 9 or Field_StatusBits bit 0, a tail jump to Effect_Release. Else
// WorldMap_RecordIndex (its answer not read), Sprite_SetAnimationBank(0x205);
// +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F = 0; the map byte of the cell
// record +0xB (x, z; unchecked) set to 0xA0 - AreaMap_Bytes +
// AreaMap_Header[0] * z + x; Sprite_SetAnimation(0) and +1 = 1.
void Record4MarkCell(const WorldMapTables& t) {
    if (At(at::kFlag3A79)[0] == 9 || (Field_StatusBits & 1) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    AH_CALL(WorldMap_RecordIndex)();
    AH_CALL(Sprite_SetAnimationBank)(0x205);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    const U width = AreaMap_Header[0];
    const U record = t.cells + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// The drift layer (the record's +0x10, through WorldMap_RecordHook10; area
// 33's WorldMap33_DrawDrift 0x4048E0 instruction for instruction):
// docs/worldmap_area.md section 4. Once (+2 == 0): the word +0x3A +=
// Frame_Counter & 0xF, +2 + 1. Nothing more unless Draw_PassFlags bit 2. With
// b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A = -8 above the map's height
// + 8; nothing more unless the leader is within 25 cells in x OR in z; one
// textured square through the GTE, then a grid of (17 - 4b) x (16 - 4b)
// semi-transparent map-item quads with u / v from the copy's drift table by e
// (unchecked).
void DrawDrift(const WorldMapTables& t) {
    unsigned char* o = Cur();
    if (o[2] == 0) {
        SetWord(o + 0x3A, Word(o + 0x3A) + (Frame_Counter & 0xF));
        Cur()[2] = static_cast<unsigned char>(Cur()[2] + 1);
        o = Cur();
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const std::int32_t b = o[0xB];
    const std::int32_t e = b - 2;
    SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x38)) + (static_cast<U>(b) << 10)));
    o = Cur();
    if (S16(o + 0x3A) > static_cast<std::int32_t>(At(at::kMapHeight)[0]) + 8) {
        SetWord(o + 0x3A, 0xFFF8);
        o = Cur();
    }
    std::int32_t dx = S16(At(at::kLeaderCellX)) - S16(o + 0x36);
    if (dx < 0) dx = -dx;
    if (dx > 0x19) {
        std::int32_t dz = S16(At(at::kLeaderCellZ)) - S16(o + 0x3A);
        if (dz < 0) dz = -dz;
        if (dz > 0x19) return;
    }

    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(prim);
    AH_CALL(Gpu_SetShadeTex)(prim, 0);
    o = Cur();
    const std::int32_t side = 2 - e;   // 4 - b
    std::int32_t r = 15 - static_cast<std::int32_t>(Frame_Counter & 0x1F);
    if (r < 0) r = -r;
    r += static_cast<std::int32_t>(static_cast<U>(side) << 8);
    const std::int32_t cx = (Long(o + 0x34) >> 9) - 0x3FC0;
    const std::int32_t cz = (Long(o + 0x38) >> 9) - 0x3FC0;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    SetWord(v + 4, 0xFD00);
    SetWord(v + 0x12, static_cast<unsigned>(cz + r));
    SetWord(v + 0x1A, static_cast<unsigned>(cz + r));
    SetWord(v + 0x08, static_cast<unsigned>(cx + r));
    SetWord(v + 0x18, static_cast<unsigned>(cx + r));
    SetWord(v + 0x00, static_cast<unsigned>(cx - r));
    SetWord(v + 0x10, static_cast<unsigned>(cx - r));
    SetWord(v + 0x02, static_cast<unsigned>(cz - r));
    SetWord(v + 0x0A, static_cast<unsigned>(cz - r));
    SetWord(v + 0x0C, 0xFD00);
    SetWord(v + 0x14, 0xFD00);
    SetWord(v + 0x1C, 0xFD00);
    long depth = 0, flag = 0;
    const auto* const sv = reinterpret_cast<const short*>(v);
    using Pers4 = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                                  long*, long*);
    AH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8),
                                                reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                                                reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    AH_CALL(Gte_PrimDepths4_10)(prim);
    AH_CALL(Prim_SetTexture)(static_cast<U>(e) | 0xBB509100u, prim, 1);
    AH_CALL(Gfx_CommitPrim)(4, 0x48);

    const std::int32_t rows = 9 - 4 * e;    // 17 - 4b
    const std::int32_t columns = 4 * side;  // 16 - 4b
    for (std::int32_t j = 0; j < rows; ++j) {
        for (std::int32_t i = 0; i < columns; ++i) {
            o = Cur();
            unsigned char* const item = AH_CALL(MapView_ItemHalfAt)(S16(o + 0x36) + i + e - 2, S16(o + 0x3A) + j + e - 2);
            if (item == nullptr) continue;
            unsigned char* const q = Gfx_PacketNext;
            AH_CALL(Gpu_SetPolyFT4)(q);
            AH_CALL(Gpu_SetShadeTex)(q, 0);
            AH_CALL(Gpu_SetSemiTrans)(q, 1);
            std::memcpy(q + 0x08, item + 0x08, 12);
            std::memcpy(q + 0x18, item + 0x18, 12);
            std::memcpy(q + 0x28, item + 0x28, 12);
            std::memcpy(q + 0x38, item + 0x38, 12);
            SetWord(q + 0x16, 0x78CB);
            SetWord(q + 0x26, 0x5B);
            q[4] = q[5] = q[6] = 0x28;
            const U m = At(t.drift_size + static_cast<U>(e))[0];
            const U ubase = At(t.drift_u + static_cast<U>(e))[0];
            const U vbase = At(t.drift_v + static_cast<U>(e))[0];
            const U scroll = (Word(Cur() + 0x38) * m) >> 16;
            const U ui = static_cast<U>(i), uj = static_cast<U>(j);
            q[0x14] = static_cast<unsigned char>(ui * m + ubase);
            q[0x15] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x24] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x25] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x34] = static_cast<unsigned char>(ui * m + ubase);
            q[0x35] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            q[0x44] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x45] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            o = Cur();
            const U x = static_cast<U>(S16(o + 0x36) + i + e - 2) << 16;
            const U z = static_cast<U>(S16(o + 0x3A) + j + e - 2) << 16;
            AH_CALL(MapView_LinkPrimAt)(x, z, 1, 0x48);
        }
    }
}

}  // namespace

// ===========================================================================
// Area 151: the world map's tenth copy (WorldMap_Records record 9)
// ===========================================================================

// original 0x423950 (WorldMap_FieldHooks[9]; 0x12B bytes): area 87's place
// hook without the cell and name-set searches - one name set, message 0x16,
// five text rows.
extern "C" void __cdecl Area151_PlaceMessage(void) { PlaceMessage151(at::kWm151); }
// original 0x423A80 (record 9 +0): the plate's run, Area151_PlateStates (state
// 0 is 0x424BA0, area 152's block).
extern "C" void __cdecl Area151_PlateRun(void) { PlateRun(at::kWm151); }
// original 0x423B60 (plate state 1): Area151_PlateAnims searched with no bound.
extern "C" void __cdecl Area151_PlateShow(void) { PlateShow(at::kWm151); }
// original 0x423CB0 / 0x423D00 / 0x423D60 (plate states 2, 3, 4).
extern "C" void __cdecl Area151_PlateGrow(void) { PlateGrow(); }
extern "C" void __cdecl Area151_PlateHold(void) { PlateHold(); }
extern "C" void __cdecl Area151_PlateShrink(void) { PlateShrink(); }
// original 0x423DB0 (record 9 +0xC): Area151_HudStates by +1 (0: WorldMapHud_Start).
extern "C" void __cdecl Area151_HudRun(void) { StateEntry("Area151", "HudRun", at::kWm151.hud_states, 2, Cur()[1])(); }
// original 0x423DD0 (HUD state 1): the frame's step, then a tail jump to the box's.
extern "C" void __cdecl Area151_HudFrame(void) {
    area_harness::Phase(at::kWm151.fn_frame_step)();
    area_harness::Phase(at::kWm151.fn_box_step)();
}
// original 0x423DE0: Area151_FrameStates by +2 (0: WorldMap_FrameWait).
extern "C" void __cdecl Area151_FrameStep(void) { StateEntry("Area151", "FrameStep", at::kWm151.frame_states, 4, Cur()[2])(); }
// original 0x423E00 / 0x423E30 / 0x423E60 (frame states 1, 2, 3).
extern "C" void __cdecl Area151_FrameSlideIn(void) { FrameSlideIn(at::kWm151); }
extern "C" void __cdecl Area151_FrameHold(void) { FrameHold(at::kWm151); }
extern "C" void __cdecl Area151_FrameSlideOut(void) { FrameSlideOut(at::kWm151); }
// original 0x423EB0: Area151_BoxStates by +3 (0: WorldMapHud_BoxWait).
extern "C" void __cdecl Area151_BoxStep(void) { StateEntry("Area151", "BoxStep", at::kWm151.box_states, 4, Cur()[3])(); }
// original 0x423ED0 / 0x423F40 / 0x423FB0 (box states 1, 2, 3).
extern "C" void __cdecl Area151_BoxSlideIn(void) { BoxSlideIn(at::kWm151); }
extern "C" void __cdecl Area151_BoxHold(void) { BoxHold(at::kWm151); }
extern "C" void __cdecl Area151_BoxSlideOut(void) { BoxSlideOut(at::kWm151); }
// original 0x424010 / 0x4241E0 / 0x4242A0: the dial frame, one sprite, the region box.
extern "C" void __cdecl Area151_DrawFrame(int x, int y) { DrawFrame(at::kWm151, x, y); }
extern "C" void __cdecl Area151_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm151, x, y, index); }
extern "C" void __cdecl Area151_DrawHud(int x, int y) { DrawHud(at::kWm151, x, y); }
// original 0x424300 (record 9 +8): Area151_Record8States by +1 (0x4253C0, ours
// 0x424320, Area65_Record8Move 0x40C490).
extern "C" void __cdecl Area151_Record8Run(void) { StateEntry("Area151", "Record8Run", at::kWm151.record8_states, 3, Cur()[1])(); }
// original 0x424320 (record-8 state 1).
extern "C" void __cdecl Area151_Record8Place(void) { Record8Place(at::kWm151); }
// original 0x424480 (record 9 +4): Area151_Record4States by +1 (ours 0x4244A0,
// Area45_Record4Tick 0x408990).
extern "C" void __cdecl Area151_Record4Run(void) { StateEntry("Area151", "Record4Run", at::kWm151.record4_states, 2, Cur()[1])(); }
// original 0x4244A0 (record-4 state 0): Area151_Cells' record +0xB marked 0xA0.
extern "C" void __cdecl Area151_Record4MarkCell(void) { Record4MarkCell(at::kWm151); }
// original 0x424560 (record 9 +0x10): the drift layer, Area151_DriftUV.
extern "C" void __cdecl Area151_DrawDrift(void) { DrawDrift(at::kWm151); }

void AreaW3g_Inject() {
    if (bof3::WantsShadow("area_w3g")) area_w3g::SelfTest();
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
    BOF3_INJECT(Area149_DustRun);
    BOF3_INJECT(Area149_DustNear880);
    BOF3_INJECT(Area149_DustNear680);
    BOF3_INJECT(Area149_DustAtZero);
    BOF3_INJECT(Area149_DustHold);
    BOF3_INJECT(Area149_SpawnDust);
    BOF3_INJECT(Area149_ViewShiftBack);
    BOF3_INJECT(Area149_Tail33);
    BOF3_INJECT(Area149_Init);
    BOF3_INJECT(Area150_ChoiceFill5B);
    BOF3_INJECT(Area150_ChoiceMessage);
    BOF3_INJECT(Area150_ChoiceTailState);
    BOF3_INJECT(Area150_Tail58);
    BOF3_INJECT(Area150_StepHook);
    BOF3_INJECT(Area151_PlaceMessage);
    BOF3_INJECT(Area151_PlateRun);
    BOF3_INJECT(Area151_PlateShow);
    BOF3_INJECT(Area151_PlateGrow);
    BOF3_INJECT(Area151_PlateHold);
    BOF3_INJECT(Area151_PlateShrink);
    BOF3_INJECT(Area151_HudRun);
    BOF3_INJECT(Area151_HudFrame);
    BOF3_INJECT(Area151_FrameStep);
    BOF3_INJECT(Area151_FrameSlideIn);
    BOF3_INJECT(Area151_FrameHold);
    BOF3_INJECT(Area151_FrameSlideOut);
    BOF3_INJECT(Area151_BoxStep);
    BOF3_INJECT(Area151_BoxSlideIn);
    BOF3_INJECT(Area151_BoxHold);
    BOF3_INJECT(Area151_BoxSlideOut);
    BOF3_INJECT(Area151_DrawFrame);
    BOF3_INJECT(Area151_DrawSprite);
    BOF3_INJECT(Area151_DrawHud);
    BOF3_INJECT(Area151_Record8Run);
    BOF3_INJECT(Area151_Record8Place);
    BOF3_INJECT(Area151_Record4Run);
    BOF3_INJECT(Area151_Record4MarkCell);
    BOF3_INJECT(Area151_DrawDrift);
}

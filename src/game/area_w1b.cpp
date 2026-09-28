// World 1's areas 42..47: the code of the PSX's BIN/WORLD01/AREA042..047.EMI
// compiled into the exe at 0x406650..0x408FE7 - 55 functions (the band's 56
// less WorldMap_DrawNeedle 0x408530, round seven's, which lies in area 45's
// block), each read to its last instruction with capstone (2026-09-27) and
// taken through the area harness (area_harness.h). Round ten group AR1B;
// docs/area_w1b.md has the areas one section each.
//
// Area 42 runs a timed round: its handlers set four story flags, a mode tail
// (kind 7) counts 900 frames down on screen and a step hook calls it off.
// Area 43 an object's tint fade and two small handlers. Area 44 two gates of
// map cells a cell hook's four switches turn, through two mode tails (kinds
// 8 and 9, armed through a register: the tool's second known gap). Area 45
// is a world map (WorldMap_Records' record 2): its code is area 16's
// (docs/area_w0b.md), instruction for instruction over its own tables, but
// for the plate's animation bank and the region label's offset cell. Area 46
// a party drop-in tail (kind 11) and its step hook; area 47 two handlers.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w1b.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;
using U = std::uint32_t;
using area_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Bank() { return At(at::kStoryFlags); }
unsigned char& B(U address) { return At(address)[0]; }
signed char TailState() { return static_cast<signed char>(B(at::kTailState)); }
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
unsigned char* ActiveMember() { return Field_ActiveMember; }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// Area 42's and 44's own functions another of them calls directly, by their
// original addresses (each is patched to ours; the fuzz stands a recorder in).
constexpr U kCheckAll42 = 0x4068D0, kSetGates44 = 0x407320, kPushParty44 = 0x4077F0;
using CheckAllFn = unsigned char (__cdecl*)();
using PushPartyFn = void (__cdecl*)(unsigned, unsigned);

// Area 45's (area 16's copy's) own functions another of them calls directly.
constexpr U kFrameStep45 = 0x408070, kFrameHold45 = 0x4080C0, kBoxStep45 = 0x408140;
constexpr U kDrawFrame45 = 0x4082A0, kDrawSprite45 = 0x408470, kDrawHud45 = 0x4086D0;
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (At(at::kMapMode)[0] != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

// The first of `count` button-table entries whose mask word has a bit of the
// button word's low 16 bits: its sprite byte, or -1.
int KeyIndex(U button_word, U count) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* const entry = At(at::kA45Buttons + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
}

}  // namespace

// ===========================================================================
// Area 42: a timed round - four story flags 0x12..0x15 set against a
// 900-frame countdown (mode tail kind 7)
// ===========================================================================

// original 0x406650 (area 42's choice 0, the descriptor 0x5F6290's +0x34, and
// its handler 6 - the choice table is the handler array's tail; PSX
// 0x801F3D90): message = Area42_Messages 0x5F62D4 by the s8 choice
// (unchecked).
extern "C" void __cdecl Area42_ChoiceMessage(void) {
    const auto choice = static_cast<signed char>(B(at::kChoice));
    SetWord(At(at::kMessage), Word(At(at::kA42Messages + static_cast<U>(choice * 2))));
}

// original 0x406670 (area 42's handler 0, Area42_Handlers 0x5F6274; PSX
// 0x801F3DBC): Field_ActiveMember's +0x80 bit 0 cleared; then, only when
// MoveScript_EffectState[the leader's +0x89] is 1 and neither story flag 0xB
// nor 0x11 is set: flag 0xB set, Field_ScriptFlags bit 5, and the mode tail
// armed - kind 7 (0x9039F3), state 0xA, the countdown word 0x9039F6 = 0x384.
extern "C" void __cdecl Area42_StartTimer(void) {
    ActiveMember()[0x80] = static_cast<unsigned char>(ActiveMember()[0x80] & 0xFE);
    if (MoveScript_EffectState[B(at::kLeaderEffect)] != 1) return;
    if (AH_CALL(Flags_Test)(Bank(), 0xB) != 0) return;
    if (AH_CALL(Flags_Test)(Bank(), 0x11) != 0) return;
    AH_CALL(Flags_Set)(Bank(), 0xB);
    reinterpret_cast<unsigned char*>(&Field_ScriptFlags)[0] |= 0x20;   // a byte or
    B(at::kTailKind) = 7;
    B(at::kTailState) = 0xA;
    SetWord(At(at::kTailTimer), 0x384);
}

// original 0x4066F0 (area 42's handler 1; PSX 0x801F3E80): Flags_Clear(story
// flags, the byte Field_ActiveMember +0xA0 plus 0x10, a byte).
extern "C" void __cdecl Area42_ClearMemberFlag(void) {
    AH_CALL(Flags_Clear)(Bank(), static_cast<unsigned char>(ActiveMember()[0xA0] + 0x10));
}

namespace {
// Area 42's handlers 2..5 (0x406710, 0x406780, 0x4067F0, 0x406860; one shape,
// the flag 0x12..0x15): Field_ActiveMember's +0x80 bit 0 cleared; only when
// MoveScript_EffectState[the leader's +0x89] is 1 and the flag is not yet
// set: the flag set, Area42_CheckAll (its answer not read), and unless flag
// 0xB is set, Field_ActiveMember's word +0x8A (read again) + 1.
void Touch42(unsigned flag) {
    ActiveMember()[0x80] = static_cast<unsigned char>(ActiveMember()[0x80] & 0xFE);
    if (MoveScript_EffectState[B(at::kLeaderEffect)] != 1) return;
    if (AH_CALL(Flags_Test)(Bank(), flag) != 0) return;
    AH_CALL(Flags_Set)(Bank(), flag);
    AH_AT(CheckAllFn, kCheckAll42)();
    if (AH_CALL(Flags_Test)(Bank(), 0xB) != 0) return;
    unsigned char* const m = ActiveMember();
    SetWord(m + 0x8A, Word(m + 0x8A) + 1u);
}
}  // namespace

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
    const unsigned seconds = Word(At(at::kTailTimer)) / 30u;
    B(at::kTailKind) = 7;
    B(at::kTailState) = 0;
    B(at::kTailArg) = 0x3C;
    const auto low = static_cast<unsigned char>(seconds);
    unsigned char rank = low > 5 ? 2 : (low == 5 ? 1 : 0);
    B(at::kRank) = rank;
    for (unsigned k = 0; k < 30; ++k) {
        unsigned char* const o = Sprite_Objects + k * 0xA4u;
        if ((o[0] & 1) == 0 || o[6] != 9 || static_cast<unsigned char>(o[5] - 0x5C) >= 3) continue;
        if (AH_CALL(Flags_Test)(At(at::kFlagsCC), static_cast<unsigned char>(rank + 0x5C)) != 0) {
            Sprite_Current = o;
            AH_CALL(Sprite_SetAnimation)(1);
            Sprite_Current = Field_ActiveMember;
            return 1;
        }
        rank = B(at::kRank);
    }
    return 1;
}

namespace {
// The countdown's two draws (states 0 and 10 of Area42_TimerTail): the
// window 0x40E750(0x7C, 0x2E, 0x48, 0x11, 0); with `jitter`, Rand() & 3 (after
// the window, as the original calls it); then Crt_sprintf(0x904BA0, the
// format 0x5F6308, seconds, hundredths) with seconds = the word / 30 and
// hundredths = (the word % 30) * 10 / 3 (plus the jitter), both as words.
// Answers the seconds' low word.
unsigned FormatTime(bool jitter) {
    const unsigned w = Word(At(at::kTailTimer));
    const unsigned seconds = w / 30u;
    const unsigned hundredths = (w % 30u) * 10u / 3u;
    AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned), area_w1b::kDrawWindow)(0x7C, 0x2E, 0x48, 0x11, 0);
    const unsigned extra = jitter ? static_cast<unsigned>(AH_CALL(Rand)()) & 3u : 0u;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextBuffer)), reinterpret_cast<const char*>(At(at::kA42Format)),
                         seconds & 0xFFFF, extra + (hundredths & 0xFFFF));
    return seconds & 0xFFFF;
}
void DrawTime(unsigned colour) {
    AH_CALL(Text_DrawAt)(0x82, 0x30, static_cast<int>(colour), 5, At(at::kTextBuffer));
}
// An effect of kind 0x13 (a camera turn, CameraTurn_Run) at Effect_FindFree's
// slot, kept in 0x9039F5: +0x64 = target, +0x68 / +0x6C = Camera_Angles[1] /
// [2] sign-extended, +9 = 0x10 frames.
void SpawnTurn(std::int32_t target) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kTailArg) = slot;
    if (slot == 0xFF) return;
    const std::int32_t a1 = S16(At(at::kCameraAngle1));
    const std::int32_t a2 = S16(At(at::kCameraAngle2));
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, target);
    SetLong(e + 0x68, a1);
    SetLong(e + 0x6C, a2);
    e[9] = 0x10;
}
// Kind 7's two ends: F3, F4 and the countdown word zeroed.
void EndTail7() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    SetWord(At(at::kTailTimer), 0);
}
}  // namespace

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
        if (B(at::kTailArg) != 0) {
            const unsigned seconds = FormatTime(false);
            DrawTime(seconds < 10 ? 2u : 0u);
            B(at::kTailArg) = static_cast<unsigned char>(B(at::kTailArg) - 1);
            return;
        }
        [[fallthrough]];
    case 1:
        Field_Kind2X = 0x150000;
        Field_Kind2Z = 0x190000;
        MoveScript_F3Divisor = 0x40;
        B(at::kTailState) = 2;
        return;
    case 2:
        if (Field_Kind2Hold != 0) return;
        MoveScript_F3Divisor = 0;
        SpawnTurn(-770);
        B(at::kTailState) = 3;
        return;
    case 3:
        if ((EffectAt(B(at::kTailArg))[0] & 1) != 0) return;
        AH_CALL(Flags_Set)(Bank(), 0x11);
        B(at::kTailState) = 9;
        return;
    case 4:
        SpawnTurn(-682);
        B(at::kTailState) = 5;
        return;
    case 5:
        if ((EffectAt(B(at::kTailArg))[0] & 1) != 0) return;
        Field_Kind2X = Long(At(at::kLeaderX));
        Field_Kind2Z = Long(At(at::kLeaderZ));
        MoveScript_F3Divisor = 0x40;
        B(at::kTailState) = 6;
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
        DrawTime(seconds < 10 ? (B(at::kTailTimer) >> 1) & 2u : 0u);
        if (Word(At(at::kTailTimer)) != 0) {
            SetWord(At(at::kTailTimer), Word(At(at::kTailTimer)) - 1u);
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
    const unsigned char kind = B(at::kTailKind);
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
                if (AH_CALL(Flags_Test)(At(at::kFlagsCC), static_cast<unsigned char>(i + 0x5C)) == 0) {
                    B(at::kRank) = static_cast<unsigned char>(i);
                    break;
                }
            }
            if (i == 3) B(at::kRank) = 2;
        }
        const auto found = static_cast<unsigned char>(AH_AT(unsigned (__cdecl*)(), area_w1b::kFreeObject)());
        SetWord(At(at::kFoundSlot), found);
        if (found != 0xFF) AH_CALL(EventOp_9x)(At(at::kA42Placements + B(at::kRank) * 13u));
    }
    if (Cond_ByteFD != 1) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kFoundSlot) = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(B(at::kFoundSlot));
    e[0] = 1;
    e[5] = 0x28;
    e[6] = 2;
    SetLong(e + 0xC, 0x418000);
    SetLong(e + 0x10, 0x108000);
    SetLong(e + 0x18, 0x4D8000);
    SetLong(e + 0x1C, 0x108000);
}

// ===========================================================================
// Area 43
// ===========================================================================

// original 0x406F80 (area 43's handler 0, Area43_Handlers 0x5F6A18): a tail
// jump through Area43_States 0x5F6A6C by Sprite_Current[4] (ours aborts past
// its two entries).
extern "C" void __cdecl Area43_ObjectRun(void) { StateEntry("Area43_ObjectRun", at::kA43States, 2, Cur()[4])(); }

// original 0x406FA0 (Area43_States 0): Field_ActiveMember +0x9F =
// Sprite_SetTint(Sprite_Current, 0, 0, 0, 1) (the member read after the
// call); Sprite_Current +0 bit 5; the tint bytes +0x5F, +0x5E, +0x5D = 0xC0;
// +0x5C = 1, +0x48 = 2, +4 = 1 (state 1); Field_ActiveMember's word +0x8A - 2
// (the object's script position held back two bytes: the op runs again).
// Sprite_Current is read again for each store.
extern "C" void __cdecl Area43_TintStart(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Cur(), 0, 0, 0, 1);
    ActiveMember()[0x9F] = tint;
    Cur()[0] = static_cast<unsigned char>(Cur()[0] | 0x20);
    Cur()[0x5F] = 0xC0;
    Cur()[0x5E] = 0xC0;
    Cur()[0x5D] = 0xC0;
    Cur()[0x5C] = 1;
    Cur()[0x48] = 2;
    Cur()[4] = 1;
    unsigned char* const m = ActiveMember();
    SetWord(m + 0x8A, Word(m + 0x8A) + 0xFFFEu);
}

// original 0x407020 (Area43_States 1): each tint byte +0x5D, +0x5E, +0x5F not
// yet 0x80 less 2; the dwords +0x40 and +0x44 + 0x800. When all three are
// 0x80: Tint_Release(Field_ActiveMember +0x9F), +0 bit 6, +4 = 0. Else
// Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area43_TintSettle(void) {
    for (const U off : {0x5Du, 0x5Eu, 0x5Fu}) {
        unsigned char* const o = Cur();
        if (o[off] != 0x80) o[off] = static_cast<unsigned char>(o[off] - 2);
    }
    AddLong(Cur() + 0x40, 0x800);
    AddLong(Cur() + 0x44, 0x800);
    const unsigned char* const o = Cur();
    if (o[0x5D] == 0x80 && o[0x5E] == 0x80 && o[0x5F] == 0x80) {
        AH_CALL(Tint_Release)(ActiveMember()[0x9F]);
        Cur()[0] = static_cast<unsigned char>(Cur()[0] | 0x40);
        Cur()[4] = 0;
        return;
    }
    unsigned char* const m = ActiveMember();
    SetWord(m + 0x8A, Word(m + 0x8A) + 0xFFFEu);
}

// original 0x4070D0 (area 43's handler 1): Effect_FindFree to the byte
// 0x903850; a slot: its record (the slot re-read) +0 = 1, kind +5 = 0x37,
// the word +0x2C = 0x64, +9 = 1, +6 = 0, +0x34 / +0x38 = Sprite_Current's,
// +0x3C = Sprite_Current's + 0x800000.
extern "C" void __cdecl Area43_SpawnEffect37(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kFoundSlot) = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(B(at::kFoundSlot));
    e[0] = 1;
    e[5] = 0x37;
    SetWord(e + 0x2C, 0x64);
    e[9] = 1;
    const unsigned char* const o = Cur();
    e[6] = 0;
    SetLong(e + 0x34, Long(o + 0x34));
    SetLong(e + 0x38, Long(o + 0x38));
    SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x3C)) + 0x800000u));
}

// original 0x407140 (area 43's handler 2): Sprite_ReleaseTint(Sprite_Current),
// Sprite_SetAnimationBank(0x1BD), Sprite_SetAnimation(0); +0x2A = 0, +0x40 =
// +0x44 = 0xE000; Field_ActiveMember +0x9F = Sprite_SetTint(Sprite_Current,
// 0, 0, 0, 1); +0x5C = 1, the tint bytes +0x5F, +0x5E, +0x5D = 0x14, +0x2B = 3.
extern "C" void __cdecl Area43_SetUpObject(void) {
    AH_CALL(Sprite_ReleaseTint)(Cur());
    AH_CALL(Sprite_SetAnimationBank)(0x1BD);
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[0x2A] = 0;
    SetLong(Cur() + 0x40, 0xE000);
    SetLong(Cur() + 0x44, 0xE000);
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Cur(), 0, 0, 0, 1);
    ActiveMember()[0x9F] = tint;
    Cur()[0x5C] = 1;
    Cur()[0x5F] = 0x14;
    Cur()[0x5E] = 0x14;
    Cur()[0x5D] = 0x14;
    Cur()[0x2B] = 3;
}

// original 0x4071E0 (Field_ObjectTriggers id 58, called (object, 0x904030)
// and reading neither; a gap function by the tool, placed in area 43's block
// by address): story flag 0x7F set, MoveCmd_TestFB(0x44, 0xB) (its answer not
// read), Sound_PlayEffect(0x103).
extern "C" void __cdecl Area43_Trigger58(void) {
    AH_CALL(Flags_Set)(Bank(), 0x7F);
    AH_CALL(MoveCmd_TestFB)(0x44, 0xB);
    AH_CALL(Sound_PlayEffect)(0x103);
}

// ===========================================================================
// Area 44: two gates of map cells, four switches
// ===========================================================================

namespace {
// The gate byte 0x90384B: gate A in bits 0..1, gate B in bits 4..5.
unsigned GateA() { return B(at::kCells44) & 3u; }
unsigned GateB() { return (B(at::kCells44) >> 4) & 3u; }
void SetByte(unsigned x, unsigned z, unsigned v) { AH_CALL(AreaMap_SetByte)(x, z, v); }
}  // namespace

// original 0x407210 (area 44's init, the descriptor 0x5F7030's +0x40). With
// Cond_ByteFD 0 or 2: the bytes 0x90384A and 0x90384B zeroed, story flags
// 0x16 and 0x17 cleared. With 1: Sprite_ObjectsExtra 0 and 1 placed at the
// cells Area44_Gate0[gate A] and Area44_Gate1[gate B] (the words +0x34 /
// +0x38 = 0x8000, +0x36 / +0x3A the cell, +0x3E = 0x80, +0x80 bit 7), then
// Area44_SetGates. Any other value: nothing.
extern "C" void __cdecl Area44_Init(void) {
    const unsigned char fd = Cond_ByteFD;
    if (fd == 0 || fd == 2) {
        B(at::kCount44) = 0;
        B(at::kCells44) = 0;
        AH_CALL(Flags_Clear)(Bank(), 0x16);
        AH_CALL(Flags_Clear)(Bank(), 0x17);
        return;
    }
    if (fd != 1) return;
    const unsigned a = GateA(), b = GateB();
    unsigned char* const e0 = At(at::kExtra0);
    unsigned char* const e1 = At(at::kExtra1);
    SetWord(e0 + 0x34, 0x8000);
    SetWord(e0 + 0x38, 0x8000);
    SetWord(e1 + 0x34, 0x8000);
    SetWord(e0 + 0x36, At(at::kA44Gate0 + a * 2)[0]);
    SetWord(e0 + 0x3A, At(at::kA44Gate0 + a * 2 + 1)[0]);
    SetWord(e0 + 0x3E, 0x80);
    e0[0x80] = static_cast<unsigned char>(e0[0x80] | 0x80);
    SetWord(e1 + 0x38, 0x8000);
    SetWord(e1 + 0x3E, 0x80);
    SetWord(e1 + 0x36, At(at::kA44Gate1 + b * 2)[0]);
    SetWord(e1 + 0x3A, At(at::kA44Gate1 + b * 2 + 1)[0]);
    e1[0x80] = static_cast<unsigned char>(e1[0x80] | 0x80);
    area_harness::Phase(kSetGates44)();
}

// original 0x407320 (called by Area44_Init and both gate tails): the gate
// byte read once. Gate A: for i 0, 1 six AreaMap_SetByte calls (a cell of 0
// opens, 0x10 closes) - A 0: (0x3C+i, 0xA / 0xB) open, (0x40+i, 0xA / 0xB)
// and (0x3C+i, 0xE / 0xF) closed; A 1: (0x40+i, 0xA / 0xB) open, (0x3C+i,
// 0xA / 0xB / 0xE / 0xF) closed; A 2 or 3: (0x3C+i, 0xE / 0xF) open,
// (0x40+i, 0xA / 0xB) and (0x3C+i, 0xA / 0xB) closed. Gate B likewise over
// (0x3E+i, 0xC / 0xD), (0x3E+i, 8 / 9) and (0x3A+i, 0xC / 0xD): B 0 opens the
// first, B 1 the second, B 2 or 3 the third. The x words are pushed with the
// caller's esi / edi high halves above them; AreaMap_SetByte reads the s16.
extern "C" void __cdecl Area44_SetGates(void) {
    const unsigned a = GateA(), b = GateB();
    for (unsigned i = 0; i < 2; ++i) {
        if (a == 0) {
            SetByte(0x3C + i, 0xA, 0);
            SetByte(0x3C + i, 0xB, 0);
            SetByte(0x40 + i, 0xA, 0x10);
            SetByte(0x40 + i, 0xB, 0x10);
            SetByte(0x3C + i, 0xE, 0x10);
            SetByte(0x3C + i, 0xF, 0x10);
        } else if (a == 1) {
            SetByte(0x40 + i, 0xA, 0);
            SetByte(0x40 + i, 0xB, 0);
            SetByte(0x3C + i, 0xA, 0x10);
            SetByte(0x3C + i, 0xB, 0x10);
            SetByte(0x3C + i, 0xE, 0x10);
            SetByte(0x3C + i, 0xF, 0x10);
        } else {
            SetByte(0x3C + i, 0xE, 0);
            SetByte(0x3C + i, 0xF, 0);
            SetByte(0x40 + i, 0xA, 0x10);
            SetByte(0x40 + i, 0xB, 0x10);
            SetByte(0x3C + i, 0xA, 0x10);
            SetByte(0x3C + i, 0xB, 0x10);
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (b == 0) {
            SetByte(0x3E + i, 0xC, 0);
            SetByte(0x3E + i, 0xD, 0);
            SetByte(0x3E + i, 8, 0x10);
            SetByte(0x3E + i, 9, 0x10);
            SetByte(0x3A + i, 0xC, 0x10);
            SetByte(0x3A + i, 0xD, 0x10);
        } else if (b == 1) {
            SetByte(0x3E + i, 8, 0);
            SetByte(0x3E + i, 9, 0);
            SetByte(0x3E + i, 0xC, 0x10);
            SetByte(0x3E + i, 0xD, 0x10);
            SetByte(0x3A + i, 0xC, 0x10);
            SetByte(0x3A + i, 0xD, 0x10);
        } else {
            SetByte(0x3A + i, 0xC, 0);
            SetByte(0x3A + i, 0xD, 0);
            SetByte(0x3E + i, 0xC, 0x10);
            SetByte(0x3E + i, 0xD, 0x10);
            SetByte(0x3E + i, 8, 0x10);
            SetByte(0x3E + i, 9, 0x10);
        }
    }
}

// original 0x407550 (Area_CellHooks' handler for area 44, through
// Area_CellHook): (x, z) cells, their low bytes compared. The first of the
// four records of Area44_Switches 0x5F7082 (x, z, facing, flag, kind) whose
// x and z are the cell's and whose facing & 0xF is the leader's +8 (read
// first): 0x57C160 toggles its story flag, Sound_PlayEffect(0x200),
// ScriptFlags_Set40, the mode tail armed with the record's kind (8 or 9,
// through a register) and state 0; al 1. None: al 0.
extern "C" unsigned char __cdecl Area44_SwitchHook(unsigned x, unsigned z) {
    const unsigned char facing = B(at::kLeaderDir);
    U rec = at::kA44Switches;
    for (; rec < at::kA44SwitchesEnd; rec += 5) {
        const unsigned char* const r = At(rec);
        if (r[0] == static_cast<unsigned char>(x) && r[1] == static_cast<unsigned char>(z) && (r[2] & 0xF) == facing) break;
    }
    if (rec >= at::kA44SwitchesEnd) return 0;
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), area_w1b::kFlagsToggle)(Bank(), At(rec)[3]);
    AH_CALL(Sound_PlayEffect)(0x200);
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = At(rec)[4];
    B(at::kTailState) = 0;
    return 1;
}

// original 0x4075D0 (Field_ModeTailKinds slot 8, armed by Area44_SwitchHook
// through a register - the tool's second known gap; a gap function by the
// tool): gate A (a) and gate B (b) read once, then a switch on the s8 state
// (0, 1, 10, 11, 20; a byte table and a jump table of six inside the
// function). Flag 0x16 is the switch the leader just toggled.
//    0  a 0 and flag 0x16 set: Area44_PushParty(3, 0), sound 0x201, the gate
//       byte (read again) & 0xF1 | 1 (a to 1), state 1. a 1 and flag 0x16
//       clear: Area44_PushParty(7, 0), sound 0x201, & 0xF0 (a to 0), state 1.
//       Otherwise state 10.
//    1  once Sprite_ObjectsExtra 0's +9 is 0: Area44_SetGates, then as 10.
//   10  b 0 and flag 0x16 set: Area44_PushParty(1, 1), sound 0x20E when the
//       state (read again) is 1 else 0x201, the gate byte & 0xF | 0x10 (b to
//       1), state 11. b 1 and flag clear: Area44_PushParty(5, 1), the same
//       sound test, & 0xF (b to 0), state 11. Otherwise: nothing in state 11;
//       sound 0x202 in state 1; state 20.
//   11  once Sprite_ObjectsExtra 1's +9 is 0: Area44_SetGates, sound 0x202,
//       state 20.
//   20  the tail ended (0x9039F3 and 0x9039F4 zeroed), ScriptFlags_Clear40.
extern "C" void __cdecl Area44_GateTailA(void) {
    const unsigned a = GateA(), b = GateB();
    switch (TailState()) {
    case 0:
        if (a == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) != 0) {
                AH_AT(PushPartyFn, kPushParty44)(3, 0);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(at::kCells44) = static_cast<unsigned char>((B(at::kCells44) & 0xF1) | 1);
                B(at::kTailState) = 1;
                return;
            }
        } else if (a == 1) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) == 0) {
                AH_AT(PushPartyFn, kPushParty44)(7, 0);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(at::kCells44) = static_cast<unsigned char>(B(at::kCells44) & 0xF0);
                B(at::kTailState) = 1;
                return;
            }
        }
        B(at::kTailState) = 0xA;
        return;
    case 1:
        if (At(at::kExtra0)[9] != 0) return;
        area_harness::Phase(kSetGates44)();
        [[fallthrough]];
    case 0xA:
        if (b == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) != 0) {
                AH_AT(PushPartyFn, kPushParty44)(1, 1);
                AH_CALL(Sound_PlayEffect)(B(at::kTailState) == 1 ? 0x20E : 0x201);
                B(at::kCells44) = static_cast<unsigned char>((B(at::kCells44) & 0xF) | 0x10);
                B(at::kTailState) = 0xB;
                return;
            }
        } else if (b == 1) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) == 0) {
                AH_AT(PushPartyFn, kPushParty44)(5, 1);
                AH_CALL(Sound_PlayEffect)(B(at::kTailState) == 1 ? 0x20E : 0x201);
                B(at::kCells44) = static_cast<unsigned char>(B(at::kCells44) & 0xF);
                B(at::kTailState) = 0xB;
                return;
            }
        }
        if (B(at::kTailState) == 0xB) return;
        if (B(at::kTailState) == 1) AH_CALL(Sound_PlayEffect)(0x202);
        B(at::kTailState) = 0x14;
        return;
    case 0xB:
        if (At(at::kExtra1)[9] != 0) return;
        area_harness::Phase(kSetGates44)();
        AH_CALL(Sound_PlayEffect)(0x202);
        B(at::kTailState) = 0x14;
        return;
    case 0x14:
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    default:
        return;
    }
}

// original 0x4077F0 (called by both gate tails; the tool found it by the
// calls): (direction, k), each a byte. Sprite_ObjectsExtra k's step +0xC /
// +0x10 = Field_DirectionSteps[direction] (x, z) >> 2 (arithmetic), +0x14 = 0,
// +9 = 0x20 frames. Then for each party member m from 1 while m is below
// Field_MemberCount (read again each time; nothing when it is 1 or 0):
// Sprite_Current made record m, and when Sprite_FindNearby's al less 0x1E
// (as an s8) is k - the member stands on that object: a step still running
// (+9 not 0) finished first (+0x34 / +0x38 / +0x3C += +9 times +0xC / +0x10 /
// +0x14), then the object's step copied to the member, +9 = 0x20, +1 = +2 =
// 1. Sprite_Current is left at the last record tried. Neither argument is
// checked (the callers pass 1, 3, 5, 7 and 0, 1).
extern "C" void __cdecl Area44_PushParty(unsigned direction, unsigned k) {
    const unsigned d = direction & 0xFF;
    const unsigned slot = k & 0xFF;
    // The original indexes both unchecked (its callers pass constants).
    if (d >= 8) bof3::Fatal("Area44_PushParty: direction %u is past Field_DirectionSteps' 8", d);
    if (slot >= 4) bof3::Fatal("Area44_PushParty: object %u is past Sprite_ObjectsExtra's 4", slot);
    unsigned char* const e = At(at::kExtra0 + slot * at::kExtraStride);
    const std::int32_t sx = static_cast<std::int32_t>(Field_DirectionSteps[d * 2]);
    const std::int32_t sz = static_cast<std::int32_t>(Field_DirectionSteps[d * 2 + 1]);
    SetLong(e + 0xC, sx >> 2);
    SetLong(e + 0x10, sz >> 2);
    SetLong(e + 0x14, 0);
    e[9] = 0x20;
    if (Field_MemberCount <= 1) return;
    unsigned char m = 1;
    do {
        Sprite_Current = At(at::kLeader + m * at::kPartyStride);
        const auto nearby = static_cast<signed char>(AH_CALL(Sprite_FindNearby)() - 0x1E);
        if (static_cast<std::int32_t>(nearby) == static_cast<std::int32_t>(slot)) {
            unsigned char* o = Cur();
            if (o[9] != 0) {
                AddLong(o + 0x34, static_cast<U>(Long(o + 0xC)) * o[9]);
                o = Cur();
                AddLong(o + 0x38, static_cast<U>(Long(o + 0x10)) * o[9]);
                o = Cur();
                AddLong(o + 0x3C, static_cast<U>(Long(o + 0x14)) * o[9]);
            }
            SetLong(Cur() + 0xC, Long(e + 0xC));
            SetLong(Cur() + 0x10, Long(e + 0x10));
            SetLong(Cur() + 0x14, Long(e + 0x14));
            Cur()[9] = 0x20;
            Cur()[1] = 1;
            Cur()[2] = 1;
        }
        ++m;
    } while (m < Field_MemberCount);
}

// original 0x407940 (Field_ModeTailKinds slot 9, armed by Area44_SwitchHook
// through a register; a gap function by the tool): Area44_GateTailA's shape
// for flag 0x17, gate B first and gate A second (a byte table and a jump
// table of six inside the function).
//    0  b 0 and flag 0x17 set: Area44_PushParty(7, 1), sound 0x201, the gate
//       byte & 0xF | 0x20 (b to 2), state 1. b 2 and flag clear:
//       Area44_PushParty(3, 1), sound 0x201, & 0xF (b to 0), state 1.
//       Otherwise state 10.
//    1  once Sprite_ObjectsExtra 1's +9 is 0: Area44_SetGates, then as 10.
//   10  a 0 and flag set: Area44_PushParty(5, 0), the gate byte & 0xF2 | 2
//       (a to 2). a 2 and flag clear: Area44_PushParty(1, 0), & 0xF0 (a to
//       0). Either: the state read, the byte stored, sound 0x20E for state 1
//       else 0x201, state 11. Otherwise: nothing in state 11; sound 0x202 in
//       state 1; state 20.
//   11  once Sprite_ObjectsExtra 0's +9 is 0: Area44_SetGates, sound 0x202,
//       state 20.
//   20  the tail ended, ScriptFlags_Clear40.
extern "C" void __cdecl Area44_GateTailB(void) {
    const unsigned a = GateA(), b = GateB();
    switch (TailState()) {
    case 0:
        if (b == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) != 0) {
                AH_AT(PushPartyFn, kPushParty44)(7, 1);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(at::kCells44) = static_cast<unsigned char>((B(at::kCells44) & 0xF) | 0x20);
                B(at::kTailState) = 1;
                return;
            }
        } else if (b == 2) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) == 0) {
                AH_AT(PushPartyFn, kPushParty44)(3, 1);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(at::kCells44) = static_cast<unsigned char>(B(at::kCells44) & 0xF);
                B(at::kTailState) = 1;
                return;
            }
        }
        B(at::kTailState) = 0xA;
        return;
    case 1:
        if (At(at::kExtra1)[9] != 0) return;
        area_harness::Phase(kSetGates44)();
        [[fallthrough]];
    case 0xA: {
        bool moved = false;
        unsigned char cells = 0;
        if (a == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) != 0) {
                AH_AT(PushPartyFn, kPushParty44)(5, 0);
                cells = static_cast<unsigned char>((B(at::kCells44) & 0xF2) | 2);
                moved = true;
            }
        } else if (a == 2) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) == 0) {
                AH_AT(PushPartyFn, kPushParty44)(1, 0);
                cells = static_cast<unsigned char>(B(at::kCells44) & 0xF0);
                moved = true;
            }
        }
        if (moved) {
            const unsigned char state = B(at::kTailState);
            B(at::kCells44) = cells;
            AH_CALL(Sound_PlayEffect)(state == 1 ? 0x20E : 0x201);
            B(at::kTailState) = 0xB;
            return;
        }
        if (B(at::kTailState) == 0xB) return;
        if (B(at::kTailState) == 1) AH_CALL(Sound_PlayEffect)(0x202);
        B(at::kTailState) = 0x14;
        return;
    }
    case 0xB:
        if (At(at::kExtra0)[9] != 0) return;
        area_harness::Phase(kSetGates44)();
        AH_CALL(Sound_PlayEffect)(0x202);
        B(at::kTailState) = 0x14;
        return;
    case 0x14:
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    default:
        return;
    }
}

// ===========================================================================
// Area 45: the world map's field hook (WorldMap_FieldHooks entry 2)
// ===========================================================================

// original 0x407B40 (WorldMap_FieldHooks 0x662DF0 entry 2; 0x56DE30 calls the
// entry of WorldMap_RecordIndex): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first of the eleven 0x20-byte rows at 0x5F74E4 whose word is the
//      place 0x937F82, 11 when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript.
//      Any other: the cell's 4-byte record of 0x5F70C0 whose (x, z) bytes are
//      the leader's cell words (read again after the call; searched with NO
//      bound), its id byte +3; the name set of 0x5F7644 whose id it is (3 when
//      none); up to four Text_Records rows from its item bytes (0xFF ends
//      them): "????????" and a 0 for an item whose byte at 0x9040EC is 0, else
//      the item's 16-byte name (0x669CD8 for item 0x16, else Item_NamePtr(0,
//      item + 0x38)); Msg_OpenScript(set + 0x16). Then 0x9039F4 (read again)
//      + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 3 (which reads the plate state table's bytes) is checked.
extern "C" void __cdecl Area45_PlaceMessage(void) {
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
        for (U a = at::kA45PlaceMessages; a < at::kA45PlaceMessagesEnd; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(at::kA45PlaceMessages + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = Word(At(at::kLeaderCellWordX));
        const unsigned cz = Word(At(at::kLeaderCellWordZ));
        U cell = at::kA45Cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = at::kA45NameSets; a < at::kA45NameSetsEnd; a += 5, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(at::kA45NameSets + 1 + set * 5);
        for (U row = at::kTextRecords; row < at::kTextRecordsEnd; row += 0x20, ++items) {
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
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(set + 0x16));
    }
    const auto next = static_cast<unsigned char>(At(at::kMsgState)[0] + 1);
    Field_Request = 2;
    At(at::kMsgState)[0] = next;
}

// ===========================================================================
// Area 45: the place plate (world-map record 2 +0: effect kind 0)
// ===========================================================================

// original 0x407CC0 (WorldMap_Records[2] +0; area 33's copy 0x403E00): the
// cell ahead of the leader - the high words of (leader +9) * (+0xC) + (+0x34)
// and of the same with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the
// kind +0xB by AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2,
// 0xAE 3, else Field_ScriptFlags2 bit 12 4, else 0; then Area45_PlateStates
// 0x5F7654 by +1 (read after the calls; ours aborts past its five entries).
extern "C" void __cdecl Area45_PlateRun(void) {
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
    StateEntry("Area45_PlateRun", at::kA45PlateStates, 5, Cur()[1])();
}

// original 0x407DA0 (Area45_PlateStates 0; area 16's 0x401DE0 but for the
// bank): +0x24 = 0x80, Sprite_SetAnimationBank(0x3B), then +0x29
// = 5, +0x2A, +0x5D, +0x5E, +0x5F = 0 and +1 = 1 (the show), Sprite_Current
// read again for each store.
extern "C" void __cdecl Area45_PlateStart(void) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(0x3B);
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
}

// original 0x407DF0 (Area45_PlateStates 1; area 33's 0x403EE0): +7 = +0xB.
// Kind 1: the entry of Area45_PlateAnims 0x5F7098 whose word is the place
// 0x937F82 - searched with NO bound - +0x18 = the place (zero-extended), its
// animation byte. Kinds 2, 3, 4: animations 3, 0, 1. Each: +0x40 = 0, +0x44 =
// 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation, +1 = 2. Other kinds: no more.
extern "C" void __cdecl Area45_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = at::kA45PlateAnims;
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

// original 0x407F40 (Area45_PlateStates 2; area 33's 0x404030):
// WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0 +0x48 = 0 and +1 = 3;
// then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area45_PlateGrow(void) {
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

// original 0x407F90 (Area45_PlateStates 3; area 33's 0x404080):
// WorldMap_PinSprite; unless Game_Mode is 1 the plate leaves (+0x48 = 2, +9 =
// 8, +1 = 4) when +0xB is not +7, or Field_Request is 5, or +7 is 1 and +0x18
// is not the place (zero-extended); a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area45_PlateHold(void) {
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

// original 0x407FF0 (Area45_PlateStates 4; area 33's 0x4040E0):
// WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0: a tail jump to
// Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to Effect_Release,
// else +0x48 = 0 and +1 = 1.
extern "C" void __cdecl Area45_PlateShrink(void) {
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

// ===========================================================================
// Area 45: the HUD task (record 2 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// original 0x408040 (record 2 +0xC; area 33's 0x404130): Area45_HudStates
// 0x5F7668 by +1 - WorldMapHud_Start (shared), Area45_HudFrame.
extern "C" void __cdecl Area45_HudRun(void) { StateEntry("Area45_HudRun", at::kA45HudStates, 2, Cur()[1])(); }

// original 0x408060 (Area45_HudStates 1; area 33's 0x404150): `call
// 0x408070; jmp 0x408140` - the frame's slide, then the region box's.
extern "C" void __cdecl Area45_HudFrame(void) {
    area_harness::Phase(kFrameStep45)();
    area_harness::Phase(kBoxStep45)();
}

// original 0x408070 (area 33's WorldMap_FrameStep's dispatch): Area45_FrameStates
// 0x5F7670 by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area45_FrameStep(void) { StateEntry("Area45_FrameStep", at::kA45FrameStates, 4, Cur()[2])(); }

// original 0x408090 (Area45_FrameStates 1): the word +0x2E += 0x10; at 0x10
// and above (s16) +2 + 1; then a tail jump to Area45_FrameHold.
extern "C" void __cdecl Area45_FrameSlideIn(void) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + 0x10u);
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(kFrameHold45)();
}

// original 0x4080C0 (Area45_FrameStates 2): the mode byte 2 makes +2 = 3;
// Area45_DrawFrame(0x10, the word +0x2E). The y pushed carries a stale high
// half the drawing never reads: ours passes the word sign-extended.
extern "C" void __cdecl Area45_FrameHold(void) {
    if (At(at::kMapMode)[0] == 2) Cur()[2] = 3;
    AH_AT(DrawAt, kDrawFrame45)(0x10, S16(Cur() + 0x2E));
}

// original 0x4080F0 (Area45_FrameStates 3): the word +0x2E -= 0x10; at -0x30
// and below +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0);
// Area45_DrawFrame(0x10, y).
extern "C" void __cdecl Area45_FrameSlideOut(void) {
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
    AH_AT(DrawAt, kDrawFrame45)(0x10, S16(o + 0x2E));
}

// original 0x408140 (area 33's WorldMapHud_BoxStep 0x404230): Area45_BoxStates
// 0x5F7680 by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold,
// _BoxSlideOut.
extern "C" void __cdecl Area45_BoxStep(void) { StateEntry("Area45_BoxStep", at::kA45BoxStates, 4, Cur()[3])(); }

// original 0x408160 (Area45_BoxStates 1; area 33's 0x404250): y +0x30 -= 10;
// at 0xC8 and below (s16) +3 + 1; when the box leaves +3 = 3;
// Area45_DrawHud(0x5C, y) - the y pushed with the object pointer's high half
// above the word, which the drawing never reads.
extern "C" void __cdecl Area45_BoxSlideIn(void) {
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
    AH_AT(DrawAt, kDrawHud45)(0x5C, S16(o + 0x30));
}

// original 0x4081D0 (Area45_BoxStates 2; area 33's 0x4042C0): while +0xB is 0
// +9 + 1, at 0x5A and above (u8) +0xB + 1; when the box leaves +3 + 1;
// Area45_DrawHud(0x5C, y).
extern "C" void __cdecl Area45_BoxHold(void) {
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
    AH_AT(DrawAt, kDrawHud45)(0x5C, S16(o + 0x30));
}

// original 0x408240 (Area45_BoxStates 3; area 33's 0x404330): y +0x30 += 10;
// at 0xF0 and above (s16) +3 = 0; unless the mode byte is set (whatever
// +0xB), Field_Request is 2 or Field_ScriptFlags bit 8, +3 = 1;
// Area45_DrawHud(0x5C, y).
extern "C" void __cdecl Area45_BoxSlideOut(void) {
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
    AH_AT(DrawAt, kDrawHud45)(0x5C, S16(o + 0x30));
}

// original 0x4082A0 (area 33's WorldMap_DrawFrame 0x404390, instruction for
// instruction; its tables 0x5F76E8 / 0x5F7690): the dial frame at (x, y),
// nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to slot 1; the dial
// (sprite 0) at (x, y); the cell under the leader (AreaMap_ByteAt of the high
// words of Field_Kind2X / Z); legend 1 at (x + 0x30, y) unless the cell is
// 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit 12, its key (the first of six
// button entries whose mask has a bit of the button word 0) at (x + 0x38, y +
// 8) as the entry's sprite + 1 (lit) or the sprite (withheld); legend 2 at (x
// + 0x30, y + 0x10) unless the cell is 0xA0 / 0xA1, its key from word 6 over
// EIGHT entries (the seventh and eighth are Area45_Record8States' words) at
// (x + 0x38, y + 0x10); legend 3 at (x + 0x30, y + 0x18) when
// Field_CellHasEvent(cell x, z) or flag bit 12, or the party set is 0xC, or
// Field_ScriptFlags bit 14; the needle at (x + 0x18, y + 0x18).
extern "C" void __cdecl Area45_DrawFrame(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_AT(DrawSpriteAt, kDrawSprite45)(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                                       static_cast<short>(Word(At(at::kLeaderCellZ))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x30, y, 1);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap0))), 6);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap6))), 8);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                            static_cast<short>(Word(At(at::kLeaderCellZ)))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (At(at::kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x30, y + 0x18, 3);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// original 0x408470 (area 33's WorldMap_DrawSprite 0x404560; its table
// 0x5F7690): one sprite of the dial page at (x, y): a draw-mode primitive
// committed to slot 1, then at Gfx_PacketNext (read again) a SPRT,
// semi-transparent when the index's low byte is not 0, colour 0x80 x 3, x
// and y as floats of the arguments' low words, CLUT 0x7B80, (w, h, u, v) from
// Area45_Sprites by index & 0xFF (unchecked); Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl Area45_DrawSprite(int x, int y, unsigned index) {
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
    const unsigned char* const entry = At(at::kA45Sprites + (index & 0xFF) * 4u);
    SetWord(prim + 0x18, entry[0]);
    SetWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4086D0 (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the region box (sprite 4) at (x, y), its cap (5) at
// (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low
// word of the dword 0x803584, read after the sprites; area 16's copy reads 0x803588).
extern "C" void __cdecl Area45_DrawHud(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, kDrawSprite45)(x, y, 4);
    AH_AT(DrawSpriteAt, kDrawSprite45)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(at::kAreaTextOffset45))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// ===========================================================================
// Area 45: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// original 0x408730 (record 2 +8): Area45_Record8States 0x5F7700 by +1 -
// 0x4253C0 and 0x40C490 (the world-map copies' shared states, not this
// group's) around Area45_Record8Place.
extern "C" void __cdecl Area45_Record8Run(void) { StateEntry("Area45_Record8Run", at::kA45Record8States, 3, Cur()[1])(); }

// original 0x408750 (Area45_Record8States 1; area 33's copy 0x4046A0):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of Area45_Directions[d];
// +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words +0x36 /
// +0x3A less each direction word >> 13 (arithmetic, 16-bit), then +0x36 (when
// +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down otherwise. Then the
// words +0x36 / +0x3A less the direction words >> 9; +0xB = 0, +1 + 1;
// Sprite_SetAnimation(Area45_Record8Anims[d] byte 0), +0x2A = its byte 1 (d
// read again after the call); a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl Area45_Record8Place(void) {
    const auto dir = [](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(at::kA45Directions + half + o[8] * 4u))); };
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
    AH_CALL(Sprite_SetAnimation)(At(at::kA45Record8Anims + Cur()[8] * 2u)[0]);
    o = Cur();
    o[0x2A] = At(at::kA45Record8Anims + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x4088B0 (record 2 +4): Area45_Record4States 0x5F7724 by +1 -
// Area45_Record4MarkCell, then Area45_Record4Tick 0x408990 (shared by ten
// world maps).
extern "C" void __cdecl Area45_Record4Run(void) { StateEntry("Area45_Record4Run", at::kA45Record4States, 2, Cur()[1])(); }

// original 0x4088D0 (Area45_Record4States 0; area 33's copy 0x404820): with
// the byte 0x903A79 9 or Field_StatusBits bit 0, a tail jump to
// Effect_Release. Else WorldMap_RecordIndex (its answer not read),
// Sprite_SetAnimationBank(0x205); +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F =
// 0; the map byte of the cell record Area45_Cells[+0xB] (x, z; unchecked) set
// to 0xA0 - AreaMap_Bytes + AreaMap_Header[0] * z + x; Sprite_SetAnimation(0)
// and +1 = 1.
extern "C" void __cdecl Area45_Record4MarkCell(void) {
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
    const U record = at::kA45Cells + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// original 0x408990 (Area45_Record4States 1, and the record +4 state 1 of ten
// world maps: areas 16, 33, 45, 65, 87, 88, 115, 121, 151, 152 - one body, in
// area 45's block): Sprite_ScriptTick (its answer not read), then a tail jump
// to Sprite_UpdateScreenSlot.
extern "C" void __cdecl Area45_Record4Tick(void) {
    AH_CALL(Sprite_ScriptTick)();
    AH_CALL(Sprite_UpdateScreenSlot)();
}

// ===========================================================================
// Area 45: the drift layer (record 2 +0x10)
// ===========================================================================

// original 0x4089A0 (record 2 +0x10, through WorldMap_RecordHook10; area 33's
// WorldMap33_DrawDrift 0x4048E0 instruction for instruction, its table
// Area45_DriftUV 0x5F772C): docs/worldmap_area.md section 4. Once (+2 == 0):
// the word +0x3A += Frame_Counter & 0xF, +2 + 1. Nothing more unless
// Draw_PassFlags bit 2. With b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A
// = -8 above the map's height + 8; nothing more unless the leader is within
// 25 cells in x OR in z; one textured square through the GTE, then a grid of
// (17 - 4b) x (16 - 4b) semi-transparent map-item quads with u / v from the
// table by e (unchecked).
extern "C" void __cdecl Area45_DrawDrift(void) {
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
            const U m = At(at::kA45DriftSize + static_cast<U>(e))[0];
            const U ubase = At(at::kA45DriftUBase + static_cast<U>(e))[0];
            const U vbase = At(at::kA45DriftVBase + static_cast<U>(e))[0];
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

// ===========================================================================
// Area 46: a party drop-in (mode tail kind 11) and its step hook
// ===========================================================================

// original 0x408E10 (Field_ModeTailKinds slot 11, armed by Area46_StepHook):
// a switch on the s8 state (a byte table and a jump table of five inside the
// function). 0: state 1. 1: once the byte 0x903849 is 2, ScriptFlags_Clear40,
// 0x903849 = 0, the tail ended. 10: Party_DropIn(1), state 11. 11: once
// 0x903849 is 0, ScriptFlags_Clear40 and the tail ended. Any other: nothing.
extern "C" void __cdecl Area46_DropTail(void) {
    switch (TailState()) {
    case 0:
        B(at::kTailState) = 1;
        return;
    case 1:
        if (B(at::kDrop46) != 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kDrop46) = 0;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 0xA:
        AH_CALL(Party_DropIn)(1);
        B(at::kTailState) = 0xB;
        return;
    case 0xB:
        if (B(at::kDrop46) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    default:
        return;
    }
}

// original 0x408EB0 (Area_StepHook's handler for area 46): (x, z) 16.16
// positions. z at most 0x218000 (signed), x's high word 0xE..0x10, and key
// item 4 not held (KeyItem_Has): ScriptFlags_Set40, 0x903849 = 1, the mode
// tail armed - kind 11, state 10 when the s8 Cond_ByteFA is 8 or more, else
// 0; al 1. Otherwise al 0.
extern "C" unsigned char __cdecl Area46_StepHook(unsigned x, unsigned z) {
    if (static_cast<std::int32_t>(z) > 0x218000) return 0;
    if (static_cast<unsigned short>((x >> 16) - 0xE) >= 3) return 0;
    if (AH_CALL(KeyItem_Has)(4) != 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    const signed char chapter = Cond_ByteFA;
    B(at::kDrop46) = 1;
    B(at::kTailKind) = 0xB;
    B(at::kTailState) = chapter >= 8 ? 0xA : 0;
    return 1;
}

// original 0x408F10 (handler 0 of areas 7, 14 and 46: one body, in area 46's
// block): Kind2_Place(0).
extern "C" void __cdecl Area46_PlaceKind2At0(void) { AH_CALL(Kind2_Place)(0); }

// original 0x408F20 (Field_ObjectTriggers id 54, called (object, 0x904030)
// and reading neither; a gap function by the tool, placed in area 46's block
// by address): story flag 0x70 set; when flags 0x6D..0x70 are all set, flag
// 0x71 too. al 0.
extern "C" unsigned char __cdecl Area46_Trigger54(void) {
    AH_CALL(Flags_Set)(Bank(), 0x70);
    for (unsigned flag = 0x6D; flag <= 0x70; ++flag)
        if (AH_CALL(Flags_Test)(Bank(), flag) == 0) return 0;
    AH_CALL(Flags_Set)(Bank(), 0x71);
    return 0;
}

// ===========================================================================
// Area 47
// ===========================================================================

// original 0x408F60 (area 47's handler 0, Area47_Handlers 0x5F8324):
// Sprite_Current +0x34 and Field_Kind2X = 0x120000, +0x38 and Field_Kind2Z =
// 0x240000; Field_ViewReset; the word +0x3E = 0x1220 and
// MapView_SetElevation(that word, read back - pushed with Field_ViewReset's
// edx above it, which MapView_SetElevation never reads: ours passes the word
// sign-extended); Camera_Angles[0] = 0xFB76, Camera_Angles[2] = 0xF8.
extern "C" void __cdecl Area47_PlaceAtStart(void) {
    SetLong(Cur() + 0x34, 0x120000);
    Field_Kind2X = 0x120000;
    SetLong(Cur() + 0x38, 0x240000);
    Field_Kind2Z = 0x240000;
    AH_CALL(Field_ViewReset)();
    SetWord(Cur() + 0x3E, 0x1220);
    AH_CALL(MapView_SetElevation)(S16(Cur() + 0x3E));
    Camera_Angles[0] = static_cast<short>(0xFB76);
    Camera_Angles[2] = 0xF8;
}

// original 0x408FC0 (area 47's handler 1): Effect_FindFree; a slot: its
// record +0 = 1, kind +5 = 0x3D.
extern "C" void __cdecl Area47_SpawnEffect3D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x3D;
}

// ===========================================================================

void AreaW1b_Inject() {
    if (bof3::WantsShadow("area_w1b")) area_w1b::SelfTest();
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
    BOF3_INJECT(Area43_ObjectRun);
    BOF3_INJECT(Area43_TintStart);
    BOF3_INJECT(Area43_TintSettle);
    BOF3_INJECT(Area43_SpawnEffect37);
    BOF3_INJECT(Area43_SetUpObject);
    BOF3_INJECT(Area43_Trigger58);
    BOF3_INJECT(Area44_Init);
    BOF3_INJECT(Area44_SetGates);
    BOF3_INJECT(Area44_SwitchHook);
    BOF3_INJECT(Area44_GateTailA);
    BOF3_INJECT(Area44_PushParty);
    BOF3_INJECT(Area44_GateTailB);
    BOF3_INJECT(Area45_PlaceMessage);
    BOF3_INJECT(Area45_PlateRun);
    BOF3_INJECT(Area45_PlateStart);
    BOF3_INJECT(Area45_PlateShow);
    BOF3_INJECT(Area45_PlateGrow);
    BOF3_INJECT(Area45_PlateHold);
    BOF3_INJECT(Area45_PlateShrink);
    BOF3_INJECT(Area45_HudRun);
    BOF3_INJECT(Area45_HudFrame);
    BOF3_INJECT(Area45_FrameStep);
    BOF3_INJECT(Area45_FrameSlideIn);
    BOF3_INJECT(Area45_FrameHold);
    BOF3_INJECT(Area45_FrameSlideOut);
    BOF3_INJECT(Area45_BoxStep);
    BOF3_INJECT(Area45_BoxSlideIn);
    BOF3_INJECT(Area45_BoxHold);
    BOF3_INJECT(Area45_BoxSlideOut);
    BOF3_INJECT(Area45_DrawFrame);
    BOF3_INJECT(Area45_DrawSprite);
    BOF3_INJECT(Area45_DrawHud);
    BOF3_INJECT(Area45_Record8Run);
    BOF3_INJECT(Area45_Record8Place);
    BOF3_INJECT(Area45_Record4Run);
    BOF3_INJECT(Area45_Record4MarkCell);
    BOF3_INJECT(Area45_Record4Tick);
    BOF3_INJECT(Area45_DrawDrift);
    BOF3_INJECT(Area46_DropTail);
    BOF3_INJECT(Area46_StepHook);
    BOF3_INJECT(Area46_PlaceKind2At0);
    BOF3_INJECT(Area46_Trigger54);
    BOF3_INJECT(Area47_PlaceAtStart);
    BOF3_INJECT(Area47_SpawnEffect3D);
}

// World 3's areas 120 and 121: the code of the PSX's BIN/WORLD03/AREA120..121
// .EMI compiled into the exe at 0x41A9D0..0x41C88C - 54 functions, each read to
// its last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR3B; docs/area_w3b.md has the
// areas one section each.
//
// Area 120: three choice handlers (two set movement-script variable 3, one
// starts run 0xD or answers message 0x35). Area 121: three handlers of its own
// (a two-state camera-distance ramp, a member reset that starts run 4, an effect
// 0xBA spawn); then the world map's ninth copy (WorldMap_Records record 8):
// area 87's code (docs/area_w2b.md section 4, itself area 45's) instruction for
// instruction over area 121's tables, but for three functions - a field hook
// that opens message (the place's index in a list of four) + 1 where area 87's
// names a place's items, the plate's animation bank 0x158, and a drift layer of
// two squares (0x295 bytes) where area 87's draws a square and a grid of map
// items. Then two engine-reached machines area 121's block holds: leader state
// 12 (Field_LeaderStates[12] 0x52FE90 jumps here in every area but 104, which
// has its own copy at 0x415020) - walk, turn, push an object, step off one, the
// menu and request buttons - and effect kind 0x5C (Effect_KindHandlers[0x5C]
// EffectKind5C_Run 0x462B60, likewise but for area 104's 0x415780): an object that follows the
// leader, turns with it, draws a two-row gauge, and four rings that rise from
// the leader. Last, EffectKind18_States[70] (a full-width band at the top of
// the screen) and Field_ObjectTriggers id 38 (arms the world map's field hook).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through a .data state table abort past the table, where the
// original would jump through whatever the next dwords hold, and effect kind
// 0x5C's start aborts on an area number past Area_Descriptors or a null
// descriptor (the owner's rule, round9 doc section 6: ours aborts where the
// original would fault; no route reaches any). The out-of-table reads and
// writes that land in mapped memory are reproduced as the original makes them
// (docs/area_w3b.md section 7). Every call goes through the harness (AH_CALL /
// AH_AT), so the start-up fuzz can stand recorders in for the callees.
#include "game/area_w3b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3b::at;
using U = std::uint32_t;
using area_harness::Handler;
using at::WorldMapTables;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr U kMessage = 0x7DEE48;   // the message word a choice handler leaves (0xFFFF: none)

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// Sprite_Objects k, or Sprite_ObjectsExtra k - 0x1E, as the original indexes
// them: k a byte, the extra records unchecked (Sprite_ObjectAt's none, 0xFF,
// lands on "extra record 0xE1", 0x80B0A4, inside .bss).
unsigned char* ObjectRecord(U k) {
    k &= 0xFF;
    return k < 0x1E ? Sprite_Objects + k * at::kObjectStride : Sprite_ObjectsExtra + (k - 0x1E) * at::kObjectStride;
}

// Field_DirectionSteps' pair for a direction - a whole byte into a table of
// eight, as the original indexes it (event_ops.cpp's StepX / StepZ).
std::uint32_t StepX(U d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }
std::uint32_t StepZ(U d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

// The script word MoveScript_Object +0xA - 2: the op runs again next frame.
void ScriptAgain() { move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 0xFFFEu); }

using ByteFn = unsigned char (__cdecl*)(void);
using StepFn = unsigned char (__cdecl*)(long, long, unsigned);
using TurnFn = unsigned char (__cdecl*)(unsigned, unsigned);
using GaugeFn = void (__cdecl*)(int, int, unsigned);
using RowFn = void (__cdecl*)(unsigned, unsigned);
// A world-map copy's own functions another of them calls directly.
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);
using Pers4 = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                              long*);

}  // namespace

// ===========================================================================
// Area 120 (its descriptor 0x623648: a choice table only)
// ===========================================================================

// original 0x41A9D0 (area 120's choice 0): no message (0xFFFF); movement-script
// variable 3 (0x903848) = 0x14 when the answer byte 0x7DEE67 is not 0, else 0xA.
extern "C" void __cdecl Area120_ChoiceVar3A(void) {
    const unsigned char answer = At(at::kChoice)[0];
    SetWord(At(kMessage), 0xFFFF);
    At(at::kScriptVar3)[0] = answer != 0 ? 0x14 : 0x0A;
}

// original 0x41A9F0 (area 120's choice 1): no message; variable 3 = 0x1E when
// the answer is not 0, else 0x28.
extern "C" void __cdecl Area120_ChoiceVar3B(void) {
    const unsigned char answer = At(at::kChoice)[0];
    SetWord(At(kMessage), 0xFFFF);
    At(at::kScriptVar3)[0] = answer != 0 ? 0x1E : 0x28;
}

// original 0x41AA10 (area 120's choice 2): answer 0 - no message, the word
// +0x8A of the record the pointer 0x903804 names + 1, run 0xD (MoveScript_Var7)
// at step 8; any other answer - message 0x35.
extern "C" void __cdecl Area120_ChoiceStartRun13(void) {
    if (At(at::kChoice)[0] != 0) {
        SetWord(At(kMessage), 0x35);
        return;
    }
    unsigned char* const record = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kRowPointer)))));
    SetWord(At(kMessage), 0xFFFF);
    SetWord(record + 0x8A, Word(record + 0x8A) + 1u);
    At(at::kScriptVar7)[0] = 0xD;
    At(at::kScriptStep)[0] = 8;
}

// ===========================================================================
// Area 121: its handlers (its descriptor 0x6245D8's +0x3C, Area121_Handlers)
// ===========================================================================

// original 0x41AA50 (area 121's handler 0; PSX 0x801F2C3C): Area121_CameraStates
// 0x62461C by Sprite_Current +4 - _CameraFar, _CameraApproach. A tail jump;
// ours aborts past the two entries.
extern "C" void __cdecl Area121_CameraRun(void) { StateEntry("Area121", "CameraRun", at::kA121CameraStates, 2, Cur()[4])(); }

// original 0x41AA70 (Area121_CameraStates 0; PSX 0x801F2C80): Camera_Distance =
// -0x424 (0xFBDC), +4 = 1, the script word - 2 (the op runs again).
extern "C" void __cdecl Area121_CameraFar(void) {
    Camera_Distance = static_cast<short>(0xFBDC);
    Cur()[4] = 1;
    ScriptAgain();
}

// original 0x41AA90 (Area121_CameraStates 1; PSX 0x801F2CBC): while
// Camera_Distance (s16) is below 0x5DC it grows by 0x40 a frame and the op
// runs again; then it is 0x5DC and +4 = 0. MapView_Redraw = 2 either way.
extern "C" void __cdecl Area121_CameraApproach(void) {
    if (Camera_Distance < 0x5DC) {
        Camera_Distance = static_cast<short>(Camera_Distance + 0x40);
        ScriptAgain();
        MapView_Redraw = 2;
        return;
    }
    Camera_Distance = 0x5DC;
    Cur()[4] = 0;
    MapView_Redraw = 2;
}

// original 0x41AAD0 (area 121's handler 1; PSX 0x801F2D30): Field_ActiveMember
// (read again for each store) +0x80 bit 0 cleared, +0x83 = 0, the word +0x8A =
// 0; Sound_PlayEffect(0x217), ScriptFlags_Set40; movement-script variables
// 3..6 (0x903848..0x90384B) = 0; run 4 (MoveScript_Var7) at step 0.
extern "C" void __cdecl Area121_StartRun4(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    Field_ActiveMember[0x83] = 0;
    SetWord(Field_ActiveMember + 0x8A, 0);
    AH_CALL(Sound_PlayEffect)(0x217);
    AH_CALL(ScriptFlags_Set40)();
    At(at::kScriptVar3)[0] = 0;
    At(at::kScriptVar4)[0] = 0;
    At(at::kScriptVar5)[0] = 0;
    At(at::kScriptVar6)[0] = 0;
    At(at::kScriptStep)[0] = 0;
    At(at::kScriptVar7)[0] = 4;
}

// original 0x41AB40 (area 121's handler 2; PSX 0x801F2DC0): Effect_FindFree; a
// slot (not 0xFF) gets +0 = 1 and kind +5 = 0xBA. The slot is not checked
// against the twenty records (Effect_FindFree answers 0..19).
extern "C" void __cdecl Area121_SpawnEffectBA(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const record = Effect_Objects + slot * at::kEffectStride;
    record[0] = 1;
    record[5] = 0xBA;
}

// ===========================================================================
// Area 121: the world map's ninth copy (area 87's code, docs/area_w2b.md
// section 4; area 45's in docs/area_w1b.md section 5; area 33's in
// docs/worldmap_area.md sections 1..4)
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

// The field hook (WorldMap_FieldHooks entry 8; area 121's own, 0x74 bytes where
// area 87's is 0x174): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the index of the place 0x937F82 in the copy's list
//      of four u16 places (4 when none); Msg_OpenScript(index + 1). Then
//      0x9039F4 (read again after the call) + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing.
void PlaceMessage(const WorldMapTables& t) {
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
    const unsigned place = Word(At(at::kPlace));
    U index = 0;
    for (U a = t.places; a < t.places_end; a += 2, ++index)
        if (Word(At(a)) == place) break;
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(index + 1));
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

// The plate's state 0: +0x24 = 0x80, Sprite_SetAnimationBank(the copy's
// bank), then +0x29 = 5, +0x2A, +0x5D, +0x5E, +0x5F = 0 and +1 = 1 (the
// show), Sprite_Current read again for each store.
void PlateStart(const WorldMapTables& t) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(t.plate_bank));
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
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
// when the box leaves +3 = 3; the copy's DrawHud(0x5C, y).
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
// area 121, as for areas 87 and 88).
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

// The drift layer (the record's +0x10, through WorldMap_RecordHook10): area
// 121's own (0x295 bytes; area 104's record names the same body). Once (+2 ==
// 0): the word +0x3A += Frame_Counter & 0xF, +2 + 1. Nothing more unless
// Draw_PassFlags bit 2. With b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A =
// -8 above the map's height + 8; nothing more unless the leader is within 25
// cells in x OR in z. About the centre ((+0x34 >> 9) - 0x3FC0, (+0x38 >> 9) -
// 0x3FC0), two textured squares through the GTE: a flat one of half-side
// (4 - b) << 8 at height 0 (Prim_SetTexture(e | 0xBB28A100, prim, 1), slot 5),
// then one at height -0x300 whose half-side adds |15 - (Frame_Counter &
// 0x1F)| (e | 0xBB509100, slot 4). Where area 87's draws one square and a
// grid of map items.
void DrawDrift() {
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
    const std::int32_t cx = (Long(o + 0x34) >> 9) - 0x3FC0;
    const std::int32_t cz = (Long(o + 0x38) >> 9) - 0x3FC0;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    const auto* const sv = reinterpret_cast<const short*>(v);
    const auto square = [v, sv](unsigned char* prim, std::int32_t x, std::int32_t z, std::int32_t r, unsigned height) {
        SetWord(v + 0x00, static_cast<unsigned>(x - r));
        SetWord(v + 0x10, static_cast<unsigned>(x - r));
        SetWord(v + 0x02, static_cast<unsigned>(z - r));
        SetWord(v + 0x0A, static_cast<unsigned>(z - r));
        SetWord(v + 0x12, static_cast<unsigned>(z + r));
        SetWord(v + 0x1A, static_cast<unsigned>(z + r));
        SetWord(v + 0x0C, height);
        SetWord(v + 0x14, height);
        SetWord(v + 0x1C, height);
        SetWord(v + 0x08, static_cast<unsigned>(x + r));
        SetWord(v + 0x18, static_cast<unsigned>(x + r));
        SetWord(v + 0x04, height);
        long depth = 0, flag = 0;
        AH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8),
                                                    reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                                                    reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
        AH_CALL(Gte_PrimDepths4_10)(prim);
    };
    const std::int32_t side = static_cast<std::int32_t>(static_cast<U>(2 - e) << 8);   // (4 - b) << 8

    unsigned char* const flat = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(flat);
    AH_CALL(Gpu_SetShadeTex)(flat, 0);
    square(flat, cx, cz, side, 0);
    AH_CALL(Prim_SetTexture)(static_cast<U>(e) | 0xBB28A100u, flat, 1);
    AH_CALL(Gfx_CommitPrim)(5, 0x48);

    unsigned char* const raised = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(raised);
    AH_CALL(Gpu_SetShadeTex)(raised, 0);
    std::int32_t pulse = 15 - static_cast<std::int32_t>(Frame_Counter & 0x1F);
    if (pulse < 0) pulse = -pulse;
    square(raised, cx, cz, side + pulse, 0xFD00);
    AH_CALL(Prim_SetTexture)(static_cast<U>(e) | 0xBB509100u, raised, 1);
    AH_CALL(Gfx_CommitPrim)(4, 0x48);
}

}  // namespace

// original 0x41AB70 (WorldMap_FieldHooks 0x662DF0 entry 8): PlaceMessage over
// area 121's place list 0x624624 (four places).
extern "C" void __cdecl Area121_PlaceMessage(void) { PlaceMessage(at::kWm121); }
// original 0x41ABF0 (WorldMap_Records[8] +0; area 87's 0x40FDE0): PlateRun
// through Area121_PlateStates 0x62462C.
extern "C" void __cdecl Area121_PlateRun(void) { PlateRun(at::kWm121); }
// original 0x41ACD0 (Area121_PlateStates 0, and area 104's plate state 0 -
// its table 0x61BB7C names this body; area 87's 0x40FEC0): PlateStart with
// bank 0x158.
extern "C" void __cdecl Area121_PlateStart(void) { PlateStart(at::kWm121); }
// original 0x41AD30 (Area121_PlateStates 1): PlateShow over Area121_PlateAnims
// 0x623690 (four entries).
extern "C" void __cdecl Area121_PlateShow(void) { PlateShow(at::kWm121); }
// original 0x41AE80 (Area121_PlateStates 2): PlateGrow.
extern "C" void __cdecl Area121_PlateGrow(void) { PlateGrow(); }
// original 0x41AED0 (Area121_PlateStates 3): PlateHold.
extern "C" void __cdecl Area121_PlateHold(void) { PlateHold(); }
// original 0x41AF30 (Area121_PlateStates 4): PlateShrink.
extern "C" void __cdecl Area121_PlateShrink(void) { PlateShrink(); }
// original 0x41AF80 (record 8 +0xC): Area121_HudStates 0x624640 by +1 -
// WorldMapHud_Start (shared), Area121_HudFrame.
extern "C" void __cdecl Area121_HudRun(void) { StateEntry("Area121", "HudRun", at::kWm121.hud_states, 2, Cur()[1])(); }
// original 0x41AFA0 (Area121_HudStates 1): `call 0x41AFB0; jmp 0x41B080` - the
// frame's slide, then the region box's.
extern "C" void __cdecl Area121_HudFrame(void) {
    area_harness::Phase(at::kWm121.fn_frame_step)();
    area_harness::Phase(at::kWm121.fn_box_step)();
}
// original 0x41AFB0 (called by Area121_HudFrame): Area121_FrameStates 0x624648
// by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area121_FrameStep(void) { StateEntry("Area121", "FrameStep", at::kWm121.frame_states, 4, Cur()[2])(); }
// original 0x41AFD0 (Area121_FrameStates 1): FrameSlideIn, then a tail jump to
// Area121_FrameHold 0x41B000.
extern "C" void __cdecl Area121_FrameSlideIn(void) { FrameSlideIn(at::kWm121); }
// original 0x41B000 (Area121_FrameStates 2): FrameHold, drawing through
// Area121_DrawFrame 0x41B1E0.
extern "C" void __cdecl Area121_FrameHold(void) { FrameHold(at::kWm121); }
// original 0x41B030 (Area121_FrameStates 3): FrameSlideOut.
extern "C" void __cdecl Area121_FrameSlideOut(void) { FrameSlideOut(at::kWm121); }
// original 0x41B080 (called by Area121_HudFrame): Area121_BoxStates 0x624658
// by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl Area121_BoxStep(void) { StateEntry("Area121", "BoxStep", at::kWm121.box_states, 4, Cur()[3])(); }
// original 0x41B0A0 (Area121_BoxStates 1): BoxSlideIn, drawing through
// Area121_DrawHud 0x41B470.
extern "C" void __cdecl Area121_BoxSlideIn(void) { BoxSlideIn(at::kWm121); }
// original 0x41B110 (Area121_BoxStates 2): BoxHold.
extern "C" void __cdecl Area121_BoxHold(void) { BoxHold(at::kWm121); }
// original 0x41B180 (Area121_BoxStates 3): BoxSlideOut.
extern "C" void __cdecl Area121_BoxSlideOut(void) { BoxSlideOut(at::kWm121); }
// original 0x41B1E0 (called by the frame states): DrawFrame over
// Area121_Buttons 0x6246C0, drawing through Area121_DrawSprite 0x41B3B0.
extern "C" void __cdecl Area121_DrawFrame(int x, int y) { DrawFrame(at::kWm121, x, y); }
// original 0x41B3B0: DrawSprite over Area121_Sprites 0x624668.
extern "C" void __cdecl Area121_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm121, x, y, index); }
// original 0x41B470 (called by the box states): DrawHud, the label's offset
// the low word of the dword 0x803580.
extern "C" void __cdecl Area121_DrawHud(int x, int y) { DrawHud(at::kWm121, x, y); }
// original 0x41B4D0 (record 8 +8): Area121_Record8States 0x6246D8 by +1 -
// 0x4253C0 and 0x40C490 (the copies' shared states, not this group's)
// around Area121_Record8Place.
extern "C" void __cdecl Area121_Record8Run(void) { StateEntry("Area121", "Record8Run", at::kWm121.record8_states, 3, Cur()[1])(); }
// original 0x41B4F0 (Area121_Record8States 1): Record8Place over
// Area121_Directions 0x6246E4 and Area121_Record8Anims 0x6246F4.
extern "C" void __cdecl Area121_Record8Place(void) { Record8Place(at::kWm121); }
// original 0x41B650 (record 8 +4): Area121_Record4States 0x6246FC by +1 -
// Area121_Record4MarkCell, then Area45_Record4Tick 0x408990 (shared).
extern "C" void __cdecl Area121_Record4Run(void) { StateEntry("Area121", "Record4Run", at::kWm121.record4_states, 2, Cur()[1])(); }
// original 0x41B670 (Area121_Record4States 0): Record4MarkCell over the cell
// record 0x6236A0.
extern "C" void __cdecl Area121_Record4MarkCell(void) { Record4MarkCell(at::kWm121); }
// original 0x41B730 (record 8 +0x10, and area 104's record 6 +0x10 - one body):
// DrawDrift, area 121's two squares.
extern "C" void __cdecl Area121_DrawDrift(void) { DrawDrift(); }

// ===========================================================================
// Area 121: leader state 12 (Field_LeaderStates[12] 0x52FE90: area 104 to
// its own 0x415020, every other area here)
// ===========================================================================

// original 0x41B9D0 (jumped to by 0x52FE9F): Area121_LeaderStates 0x624704 by
// Sprite_Current +2 - _LeaderControl, _LeaderStep - called (ours aborts past
// the two entries), then a tail jump to 0x415640's neighbour 0x4156C0 (area
// 104's block: +0xA counted down, the charge on +0xB).
extern "C" void __cdecl Area121_LeaderRun(void) {
    StateEntry("Area121", "LeaderRun", at::kLeaderStates, 2, Cur()[2])();
    AH_AT(Handler, area_w3b::kLeaderCharge104)();
}

// original 0x41B9F0 (Area121_LeaderStates 0; also Area121_LeaderStep's tail):
// nothing while Area121_StepOffObject answers, Field_ScriptFlags bit 8,
// Field_ScriptFlags2 bit 6, Field_Request or Sprite_Current +0xA. With any
// Field_InputHeld, Field_State +0x136 = 0 unless the actor record
// Field_State +0x148 (Field_ActorStates, 0xA4 each, unchecked) has bit 5.
// Then nothing while Field_LeaderCellEvent, Area121_MenuButton,
// Field_LeaderTalkTest or Area121_Request4Button answers. The facing before
// (old) and Area121_TurnInput: with neither a turn nor the push button (the
// button word 0x903582 against Input_Held), or with +0xB at 0x40, the leader
// halts (+8 = old, Field_State +0x137 = 0, 0x415640, +9 = +2 = 0). Else the
// button held puts +8 back to old; a new facing more than one step (of the
// eight, around the wrap) from old turns one step toward it; +8 &= 7. Then
// Field_LeaderStepTarget: 1 or 0xFF - +8 = old, +0x137 = 0, Field_State +0x128
// = 3, +9 = +2 = 0; 0 - Field_LeaderPushObjects answering: +0x137 = +2 = 0 and
// Area121_PushObject; else 0x415680 (the pace 3 or 4) and, at pace 4 with a
// new facing, +8 = old, +0x137 = +2 = 0; else Field_JumpStart,
// Field_JumpCheckHeight, +9 - 1, Field_LeaderStepTick, +0x137 = +2 = 1 (the
// step). Any other target: nothing.
extern "C" void __cdecl Area121_LeaderControl(void) {
    if (AH_AT(ByteFn, area_w3b::kStepOffObject)() != 0) return;
    if ((Field_ScriptFlags & 0x100) != 0) return;
    if ((Field_ScriptFlags2 & 0x40) != 0) return;
    if (Field_Request != 0) return;
    if (Cur()[0xA] != 0) return;
    if (Field_InputHeld != 0) {
        unsigned char* const state = Field_State;
        if ((Field_ActorStates[state[0x148] * at::kObjectStride] & 0x20) == 0) state[0x136] = 0;
    }
    if (AH_CALL(Field_LeaderCellEvent)() != 0) return;
    if (AH_AT(ByteFn, area_w3b::kMenuButton)() != 0) return;
    if (AH_CALL(Field_LeaderTalkTest)() != 0) return;
    if (AH_AT(ByteFn, area_w3b::kRequest4Button)() != 0) return;
    const unsigned char old = Cur()[8];
    const unsigned char turned = AH_AT(ByteFn, area_w3b::kTurnInput)();
    unsigned char* o = Cur();
    const bool pressed = (Word(At(at::kButtonMap1)) & Input_Held & 0xFFFFu) != 0;
    if ((turned == 0 && !pressed) || o[0xB] == 0x40) {
        o[8] = old;
        Field_State[0x137] = 0;
        AH_AT(Handler, area_w3b::kLeaderHalt104)();
        Cur()[9] = 0;
        Cur()[2] = 0;
        return;
    }
    if (pressed) {
        o[8] = old;
        o = Cur();
    }
    const auto now = static_cast<signed char>(o[8]);
    const auto was = static_cast<signed char>(old);
    if (now != was) {
        signed char cl = now;
        signed char tmp = was;
        signed char al = was;
        std::int32_t d = static_cast<std::int32_t>(was) - static_cast<std::int32_t>(now);
        if (d < 0) d = -d;
        if (d > 2) {
            if (was >= now) {
                cl = static_cast<signed char>(static_cast<unsigned char>(now) + 8);
            } else {
                al = static_cast<signed char>(static_cast<unsigned char>(old) + 8);
                tmp = al;
            }
        }
        std::int32_t d2 = static_cast<std::int32_t>(al) - static_cast<std::int32_t>(cl);
        if (d2 < 0) d2 = -d2;
        if (d2 == 2) {
            o[8] = static_cast<unsigned char>(tmp > cl ? old - 1 : old + 1);
            o = Cur();
        }
        o[8] = static_cast<unsigned char>(o[8] & 7);
    }
    const unsigned char target = AH_CALL(Field_LeaderStepTarget)();
    if (target == 1 || target == 0xFF) {
        Cur()[8] = old;
        Field_State[0x137] = 0;
        Field_State[0x128] = 3;
        Cur()[9] = 0;
        Cur()[2] = 0;
        return;
    }
    if (target != 0) return;
    if (AH_CALL(Field_LeaderPushObjects)() != 0) {
        Field_State[0x137] = 0;
        Cur()[2] = 0;
        AH_AT(Handler, area_w3b::kPushObject)();
        return;
    }
    AH_AT(Handler, area_w3b::kLeaderPace104)();
    if (Field_State[0x128] == 4 && Cur()[8] != old) {
        Cur()[8] = old;
        Field_State[0x137] = 0;
        Cur()[2] = 0;
        return;
    }
    AH_CALL(Field_JumpStart)();
    AH_CALL(Field_JumpCheckHeight)();
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Field_LeaderStepTick)();
    Field_State[0x137] = 1;
    Cur()[2] = 1;
}

// original 0x41BC60 (called by Area121_LeaderControl after
// Field_LeaderPushObjects answered): in area 0x79 only. The object two steps
// ahead of Sprite_Current (+0x34 / +0x38 plus twice Field_DirectionSteps[+8],
// the whole byte) by Sprite_ObjectAt(x, z, 1): with the push button held and
// the object's facing (+8 & 7) the leader's, the object's +0x80 bit 0 set -
// Sprite_Objects k, or Sprite_ObjectsExtra k - 0x1E, "none" (0xFF) included:
// extra record 0xE1, at 0x80B0A4. Otherwise Area121_StepAround(x, z, k)
// answering: Field_JumpStart, +9 - 1, Field_LeaderStepTick, Field_State +0x137
// = +2 = 1.
extern "C" void __cdecl Area121_PushObject(void) {
    if (Game_AreaNumber != 0x79) return;
    const unsigned char* o = Cur();
    const U d = o[8];
    const auto x = static_cast<long>(static_cast<U>(Long(o + 0x34)) + StepX(d) * 2u);
    const auto z = static_cast<long>(static_cast<U>(Long(o + 0x38)) + StepZ(d) * 2u);
    const unsigned char k = AH_CALL(Sprite_ObjectAt)(x, z, 1);
    const bool pressed = (Word(At(at::kButtonMap1)) & Input_Held & 0xFFFFu) != 0;
    if (pressed) {
        unsigned char* const object = ObjectRecord(k);
        if ((object[8] & 7) == Cur()[8]) {
            object[0x80] = static_cast<unsigned char>(object[0x80] | 1);
            return;
        }
    }
    if (AH_AT(StepFn, area_w3b::kStepAround)(x, z, k) == 0) return;
    AH_CALL(Field_JumpStart)();
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Field_LeaderStepTick)();
    Field_State[0x137] = 1;
    Cur()[2] = 1;
}

// original 0x41BDA0 (x, z, k; called by Area121_PushObject and
// Area121_StepOffObject): k 0xFF (its low byte) answers 0. Else +8 =
// Area121_DirectionTo(x, z, k), and up to eight times: Field_LeaderStepTarget 0
// and Sprite_ObjectAt(x, z, 0) none (0xFF) answer 1; else +8 = (+8 + 1) & 7.
// Eight turns without: 0.
extern "C" unsigned char __cdecl Area121_StepAround(long x, long z, unsigned k) {
    if ((k & 0xFF) == 0xFF) return 0;
    const unsigned char direction = AH_AT(StepFn, area_w3b::kDirectionTo)(x, z, k);
    Cur()[8] = direction;
    for (unsigned i = 0; i < 8; ++i) {
        if (AH_CALL(Field_LeaderStepTarget)() == 0 && AH_CALL(Sprite_ObjectAt)(x, z, 0) == 0xFF) return 1;
        unsigned char* const o = Cur();
        o[8] = static_cast<unsigned char>((o[8] + 1) & 7);
    }
    return 0;
}

// original 0x41BE10 (x, z, k; called by Area121_StepAround, and by area 104's
// code at 0x415406 - one body): the facing from object k (Sprite_Objects k, or
// Sprite_ObjectsExtra k - 0x1E; k its low byte) toward (x, z), by the signs of
// dx = x - +0x34 and dz = z - +0x38 (32-bit, wrapping): dx < 0: dz < 0 0, dz >
// 0 6, dz 0 7; dx > 0: dz > 0 4, dz < 0 2, dz 0 3; dx 0: dz > 0 5, else 1.
extern "C" unsigned char __cdecl Area121_DirectionTo(long x, long z, unsigned k) {
    const unsigned char* const object = ObjectRecord(k);
    const auto dx = static_cast<std::int32_t>(static_cast<U>(x) - static_cast<U>(Long(object + 0x34)));
    const auto dz = static_cast<std::int32_t>(static_cast<U>(z) - static_cast<U>(Long(object + 0x38)));
    if (dx < 0) return dz < 0 ? 0 : dz > 0 ? 6 : 7;
    if (dx > 0) return dz > 0 ? 4 : dz < 0 ? 2 : 3;
    return dz > 0 ? 5 : 1;
}

// original 0x41BEC0 (called by Area121_LeaderControl first): in area 0x79 only,
// the object on Sprite_Current's own point (Sprite_ObjectAt(+0x34, +0x38, 1));
// Area121_StepAround(+0x34, +0x38 - read again - , it) answering: the step as
// Area121_PushObject's (Field_JumpStart, +9 - 1, Field_LeaderStepTick,
// Field_State +0x137 = +2 = 1), al 1. Else al 0.
extern "C" unsigned char __cdecl Area121_StepOffObject(void) {
    if (Game_AreaNumber != 0x79) return 0;
    const unsigned char* o = Cur();
    const unsigned char k = AH_CALL(Sprite_ObjectAt)(Long(o + 0x34), Long(o + 0x38), 1);
    o = Cur();
    if (AH_AT(StepFn, area_w3b::kStepAround)(Long(o + 0x34), Long(o + 0x38), k) == 0) return 0;
    AH_CALL(Field_JumpStart)();
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Field_LeaderStepTick)();
    Field_State[0x137] = 1;
    Cur()[2] = 1;
    return 1;
}

// original 0x41BF40 (Area121_LeaderStates 1): Field_State +0x137 = 1. While +9
// is not 0: +9 - 1 and a tail jump to Field_LeaderStepTick. At 0:
// Field_Bit20Tick, Field_FloorDamage; with Field_InputFlags bit 0, the cell
// under the leader (the words +0x36 / +0x3A) by AreaMap_ByteAt: 0xAF -
// Field_ChangeArea(the place 0x937F82, the pending x 0x903860, z 0x90384C,
// flags 0x905B88), 0x904EE0 = 0, 0x937F98 = 0xC, done; 0xC0 (asked again) -
// Area_LinkAt(x, z). Then Field_EdgeBits (a word) + 1, + 1 more for the
// facings 2 and 6; 0x415640 (the halt); Scenario_ArriveHook(+0x34, +0x38)
// answering (all of eax): +2 = 0, done. Else Field_Bit80Tick; unless
// Field_ScriptFlags bit 8 or Field_ScriptFlags2 bit 6, Field_InputHeld with a
// bit of button words 0 / 1 or of 0xF000 goes on at once: a tail jump to
// Area121_LeaderControl. Else Field_State +0x137 = +2 = 0.
extern "C" void __cdecl Area121_LeaderStep(void) {
    Field_State[0x137] = 1;
    unsigned char* o = Cur();
    if (o[9] != 0) {
        o[9] = static_cast<unsigned char>(o[9] - 1);
        AH_CALL(Field_LeaderStepTick)();
        return;
    }
    AH_CALL(Field_Bit20Tick)();
    AH_CALL(Field_FloorDamage)();
    if ((Field_InputFlags & 1) != 0) {
        o = Cur();
        if (AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(o + 0x36)), static_cast<short>(Word(o + 0x3A))) == 0xAF) {
            AH_CALL(Field_ChangeArea)(Word(At(at::kPlace)), Long(At(at::kPendingX)), Long(At(at::kPendingZ)), At(at::kChangeFlags)[0]);
            At(at::k904EE0)[0] = 0;
            At(at::k937F98)[0] = 0xC;
            return;
        }
        o = Cur();
        if (AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(o + 0x36)), static_cast<short>(Word(o + 0x3A))) == 0xC0) {
            o = Cur();
            AH_CALL(Area_LinkAt)(Word(o + 0x36), Word(o + 0x3A));
        }
    }
    const unsigned char facing = Cur()[8];
    Field_EdgeBits = static_cast<unsigned short>(Field_EdgeBits + 1);
    if (facing == 2 || facing == 6) Field_EdgeBits = static_cast<unsigned short>(Field_EdgeBits + 1);
    AH_AT(Handler, area_w3b::kLeaderHalt104)();
    o = Cur();
    if (AH_CALL(Scenario_ArriveHook)(Long(o + 0x34), Long(o + 0x38)) != 0) {
        Cur()[2] = 0;
        return;
    }
    AH_CALL(Field_Bit80Tick)();
    if ((Field_ScriptFlags & 0x100) == 0 && (Field_ScriptFlags2 & 0x40) == 0) {
        const unsigned buttons = Word(At(at::kButtonMap1)) | Word(At(at::kButtonMap0)) | 0xF000u;
        if ((Field_InputHeld & buttons) != 0) {
            AH_AT(Handler, area_w3b::kLeaderControl)();
            return;
        }
    }
    Field_State[0x137] = 0;
    Cur()[2] = 0;
}

// original 0x41C0A0 (called by Area121_LeaderControl, and by area 104's code at
// 0x4150C4 - one body): Field_MenuButton pressed (Input_Pressed) outside
// Field_ScriptFlags bit 6: Sound_PlayEffect(0x105), Field_Request = 1, +2 =
// 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area121_MenuButton(void) {
    if ((Field_MenuButton & Input_Pressed) == 0) return 0;
    if ((Field_ScriptFlags & 0x40) != 0) return 0;
    AH_CALL(Sound_PlayEffect)(0x105);
    Field_Request = 1;
    Cur()[2] = 0;
    return 1;
}

// original 0x41C0E0 (called by Area121_LeaderControl, and by area 104's code at
// 0x4150DE): outside Field_ScriptFlags bit 13, the button word 0x903586 held
// (Field_InputHeld): Field_Request = 4, +2 = 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area121_Request4Button(void) {
    if ((Field_ScriptFlags & 0x2000) != 0) return 0;
    if ((Word(At(at::kButtonMap3)) & Field_InputHeld) == 0) return 0;
    Field_Request = 4;
    Cur()[2] = 0;
    return 1;
}

// original 0x41C110 (called by Area121_LeaderControl, and by area 104's code at
// 0x4150F4): by Input_Held's high byte, first match - bit 14 turns +8 about
// (^ 4), bit 13 one step (+ 1), bit 15 one step back (- 1), bit 12 keeps it;
// each then +8 &= 7 and al 1. None: al 0.
extern "C" unsigned char __cdecl Area121_TurnInput(void) {
    const unsigned held = Input_Held;
    unsigned char* const o = Cur();
    if ((held & 0x4000) != 0) o[8] = static_cast<unsigned char>(o[8] ^ 4);
    else if ((held & 0x2000) != 0) o[8] = static_cast<unsigned char>(o[8] + 1);
    else if ((held & 0x8000) != 0) o[8] = static_cast<unsigned char>(o[8] - 1);
    else if ((held & 0x1000) == 0) return 0;
    Cur()[8] = static_cast<unsigned char>(Cur()[8] & 7);
    return 1;
}

// ===========================================================================
// Area 121: effect kind 0x5C (Effect_KindHandlers[0x5C] EffectKind5C_Run 0x462B60: area 104 to
// its own 0x415780, every other area here)
// ===========================================================================

// original 0x41C190 (jumped to by 0x462B6F): Area121_Kind5CStates 0x62470C by
// Sprite_Current +1 - _Kind5CStart, _Kind5CFace, _Kind5CTurn, _RingWait,
// _RingRise. A tail jump; ours aborts past the five entries.
extern "C" void __cdecl Area121_Kind5CRun(void) { StateEntry("Area121", "Kind5CRun", at::kKind5CStates, 5, Cur()[1])(); }

// original 0x41C1B0 (Area121_Kind5CStates 0): +8 = the leader's facing;
// Sprite_InitFromEntry(the dword +8 of Area_Descriptors[Game_AreaNumber], + 8);
// +0x5D..+0x5F = 0x80; +1 + 1; Area121_Kind5CFollow. Then, unless the area is
// 0x68 without key item 0xA (KeyItem_Has), four effects: each Effect_FindFree
// slot - unchecked: none (0xFF) is record 255, at 0x7E9160 - gets +0 = 1, kind
// +5 = 0x5C, +1 = 3 (a ring's wait), +0xA = 0, 8, 0x10, 0x18 (its delay).
// Ours aborts on an area number past the 200 descriptors or a null descriptor,
// where the original would read through it.
extern "C" void __cdecl Area121_Kind5CStart(void) {
    Cur()[8] = At(at::kLeaderDir)[0];
    const unsigned area = Game_AreaNumber;
    if (area >= 200) bof3::Fatal("Area121_Kind5CStart: area %u is past Area_Descriptors' 200", area);
    const unsigned char* const descriptor = Area_Descriptors[area];
    if (descriptor == nullptr) bof3::Fatal("Area121_Kind5CStart: area %u has no descriptor", area);
    unsigned char* const entry = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(descriptor + 8)) + 8u));
    AH_CALL(Sprite_InitFromEntry)(entry);
    Cur()[0x5D] = 0x80;
    Cur()[0x5E] = 0x80;
    Cur()[0x5F] = 0x80;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    AH_AT(Handler, area_w3b::kKind5CFollow)();
    if (Game_AreaNumber == 0x68 && AH_CALL(KeyItem_Has)(0xA) == 0) return;
    for (U i = 0; i < 4; ++i) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        unsigned char* const record = Effect_Objects + slot * at::kEffectStride;
        record[0] = 1;
        record[5] = 0x5C;
        record[1] = 3;
        record[0xA] = static_cast<unsigned char>(i << 3);
    }
}

// original 0x41C270 (Area121_Kind5CStart, _Kind5CFace, _Kind5CTurn): the
// leader's +0x34, +0x38 and word +0x3E to Sprite_Current's (read again for
// each); Sprite_UpdateScreenA; then (a tail jump to 0x41C2B0, a body only
// this jump reaches, taken here) the gauge, nothing unless Draw_PassFlags &
// 0x1B: Area121_GaugeSprite(0xDC, 0x10, n) and 0x415940 rows - under
// Field_StatusBits bit 6, n 0 and rows (0x40, 0), (0x40, 1); with the leader's
// +0xA 0, n 0 and rows (0x40, 0), (0x40 - the leader's +0xB, 1); else n 1 or 2
// by Frame_Counter bit 1 and the row (0x40, 0).
extern "C" void __cdecl Area121_Kind5CFollow(void) {
    SetLong(Cur() + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    SetWord(Cur() + 0x3E, Word(At(at::kLeaderHeight)));
    AH_CALL(Sprite_UpdateScreenA)();
    if ((Draw_PassFlags & 0x1B) == 0) return;
    const auto gauge = AH_AT(GaugeFn, area_w3b::kGaugeSprite);
    const auto row = AH_AT(RowFn, area_w3b::kGaugeRow104);
    if ((Field_StatusBits & 0x40) != 0) {
        gauge(0xDC, 0x10, 0);
        row(0x40, 0);
        row(0x40, 1);
        return;
    }
    if (At(at::kLeaderA)[0] == 0) {
        gauge(0xDC, 0x10, 0);
        row(0x40, 0);
        row(static_cast<unsigned char>(0x40 - At(at::kLeaderB)[0]), 1);
        return;
    }
    gauge(0xDC, 0x10, (Frame_Counter & 2) != 0 ? 1u : 2u);
    row(0x40, 0);
}

// original 0x41C350 (x, y, n; called by Area121_Kind5CFollow, and by area 104's
// code at 0x4158C8 / 0x415902 / 0x415920): a draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to slot 2; at
// Gfx_PacketNext (read again) a SPRT: x and y as floats of the arguments' low
// words, u 0, v (n << 5) + 0x78 (a byte), 0x50 x 0x20, CLUT 0x7B80, colour 0x80
// x 3; Gfx_CommitPrim(2, 0x1C).
extern "C" void __cdecl Area121_GaugeSprite(int x, int y, unsigned n) {
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetSprt)(prim);
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, 4);
    prim[0x15] = static_cast<unsigned char>((n << 5) + 0x78);
    std::memcpy(prim + 0xC, &fy, 4);
    SetWord(prim + 0x18, 0x50);
    SetWord(prim + 0x1A, 0x20);
    SetWord(prim + 0x16, 0x7B80);
    prim[0x14] = 0;
    prim[4] = prim[5] = prim[6] = 0x80;
    AH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x41C3E0 (Area121_Kind5CStates 1; and Area121_Kind5CTurn's tail):
// while +8 is not the leader's facing, Area121_Kind5CTurnStep(+8, the
// leader's) or (from the other side) (+8 ^ 4, the leader's) answering ends it;
// neither: the leader's facing = +8. Then (and when they match) a tail jump to
// Area121_Kind5CFollow.
extern "C" void __cdecl Area121_Kind5CFace(void) {
    if (Cur()[8] != At(at::kLeaderDir)[0]) {
        if (AH_AT(TurnFn, area_w3b::kKind5CTurnStep)(Cur()[8], At(at::kLeaderDir)[0]) != 0) return;
        if (AH_AT(TurnFn, area_w3b::kKind5CTurnStep)(Cur()[8] ^ 4u, At(at::kLeaderDir)[0]) != 0) return;
        At(at::kLeaderDir)[0] = Cur()[8];
    }
    AH_AT(Handler, area_w3b::kKind5CFollow)();
}

// original 0x41C440 (a, b; called by Area121_Kind5CFace): the facings a and b
// as bytes; more than 2 apart, the smaller + 8 (around the wrap). Still more
// than 2 apart, or equal: al 0. Else a turn of one step toward b: +0x14 = -0x40
// and +8 - 1 (a above b) or +0x14 = 0x40 and +8 + 1; +8 &= 7; the leader's
// facing = +8; +9 = 8; Area121_Kind5CTurn; +1 = 2; al 1.
extern "C" unsigned char __cdecl Area121_Kind5CTurnStep(unsigned a, unsigned b) {
    auto a8 = static_cast<unsigned char>(a);
    auto b8 = static_cast<unsigned char>(b);
    std::int32_t d = static_cast<std::int32_t>(a8) - static_cast<std::int32_t>(b8);
    if (d < 0) d = -d;
    if (d > 2) {
        if (a8 < b8) a8 = static_cast<unsigned char>(a8 + 8);
        else b8 = static_cast<unsigned char>(b8 + 8);
    }
    d = static_cast<std::int32_t>(a8) - static_cast<std::int32_t>(b8);
    if (d < 0) d = -d;
    if (d > 2 || a8 == b8) return 0;
    if (a8 > b8) {
        SetLong(Cur() + 0x14, -0x40);
        Cur()[8] = static_cast<unsigned char>(Cur()[8] - 1);
    } else {
        SetLong(Cur() + 0x14, 0x40);
        Cur()[8] = static_cast<unsigned char>(Cur()[8] + 1);
    }
    Cur()[8] = static_cast<unsigned char>(Cur()[8] & 7);
    unsigned char* const o = Cur();
    At(at::kLeaderDir)[0] = o[8];
    o[9] = 8;
    AH_AT(Handler, area_w3b::kKind5CTurn)();
    Cur()[1] = 2;
    return 1;
}

// original 0x41C510 (Area121_Kind5CStates 2; called by _Kind5CTurnStep): the
// angle +0x6C += +0x14, & 0xFFF; +9 - 1; at 0 +1 = 1 and a tail jump to
// Area121_Kind5CFace, else to Area121_Kind5CFollow.
extern "C" void __cdecl Area121_Kind5CTurn(void) {
    SetLong(Cur() + 0x6C, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x6C)) + static_cast<U>(Long(Cur() + 0x14))));
    SetLong(Cur() + 0x6C, Long(Cur() + 0x6C) & 0xFFF);
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] == 0) {
        o[1] = 1;
        AH_AT(Handler, area_w3b::kKind5CFace)();
        return;
    }
    AH_AT(Handler, area_w3b::kKind5CFollow)();
}

// original 0x41C560 (Area121_Kind5CStates 3, a ring's wait): +0xA counts down;
// at 0 the ring takes the leader's +0x34, +0x38 and +0x3C + 0x800000,
// Area121_RingRise, +1 + 1.
extern "C" void __cdecl Area121_RingWait(void) {
    unsigned char* const o = Cur();
    if (o[0xA] != 0) {
        o[0xA] = static_cast<unsigned char>(o[0xA] - 1);
        return;
    }
    SetLong(o + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    SetLong(Cur() + 0x3C, static_cast<std::int32_t>(static_cast<U>(Long(At(at::kLeaderY))) + 0x800000u));
    AH_AT(Handler, area_w3b::kRingRise)();
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// original 0x41C5B0 (Area121_Kind5CStates 4, and area 104's table 0x61BC40 and
// its code at 0x415BC7 - one body): the word +0x3E += 0x10, above 0x280 (s16)
// +1 = 3 (the wait again). Nothing more unless Draw_PassFlags & 0x1B.
// Gte_PushMatrix; a matrix from rotation 0 (Gte_RotMatrix) with the translation
// Gte_RotTrans of ((+0x34 >> 9) - 0x4000, (+0x38 >> 9) - 0x4000, -(h / 2)), h
// the word +0x3E (truncated toward 0), times Camera_Matrix (Gte_MulMatrix0),
// set (Gte_SetRotMatrix, Gte_SetTransMatrix); a semi-transparent POLY_FT4
// diamond of radius (h >> 7) * 10 + 0x10 (u 0xE0..0xFE, v 0x30..0x4E, CLUT
// 0x78CA, tpage 0x3B, colour 0x80) through Gte_RotTransPers4 and
// Gte_PrimDepths4_10, MapView_LinkPrimAt(+0x34, +0x38, 1, 0x48); Gte_PopMatrix.
extern "C" void __cdecl Area121_RingRise(void) {
    SetWord(Cur() + 0x3E, Word(Cur() + 0x3E) + 0x10u);
    unsigned char* o = Cur();
    if (S16(o + 0x3E) > 0x280) o[1] = 3;
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_CALL(Gte_PushMatrix)();
    o = Cur();
    short rotation[4] = {0, 0, 0, 0};
    short position[4] = {};
    position[0] = static_cast<short>((Long(o + 0x34) >> 9) - 0x4000);
    position[1] = static_cast<short>((Long(o + 0x38) >> 9) - 0x4000);
    position[2] = static_cast<short>(-(S16(o + 0x3E) / 2));
    // a MATRIX: nine shorts, a pad, then the translation's three longs at +0x14
    alignas(4) unsigned char matrix[0x20] = {};
    AH_CALL(Gte_RotTrans)(position, reinterpret_cast<long*>(matrix + 0x14));
    AH_CALL(Gte_RotMatrix)(rotation, reinterpret_cast<short*>(matrix));
    AH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(matrix), reinterpret_cast<short*>(matrix));
    AH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    AH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    const std::int32_t radius = (S16(Cur() + 0x3E) >> 7) * 10 + 0x10;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    SetWord(v + 0x02, 0);
    SetWord(v + 0x04, 0);
    SetWord(v + 0x08, 0);
    SetWord(v + 0x0C, 0);
    SetWord(v + 0x10, 0);
    SetWord(v + 0x14, 0);
    SetWord(v + 0x1A, 0);
    SetWord(v + 0x1C, 0);
    SetWord(v + 0x00, static_cast<unsigned>(-radius));
    SetWord(v + 0x0A, static_cast<unsigned>(-radius));
    SetWord(v + 0x12, static_cast<unsigned>(radius));
    SetWord(v + 0x18, static_cast<unsigned>(radius));
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 1);
    prim[0x24] = 0xFE;
    prim[0x44] = 0xFE;
    SetWord(prim + 0x26, 0x3B);
    SetWord(prim + 0x16, 0x78CA);
    prim[0x14] = 0xE0;
    prim[0x34] = 0xE0;
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x35] = 0x4E;
    prim[0x45] = 0x4E;
    const auto* const sv = reinterpret_cast<const short*>(v);
    long depth = 0, flag = 0;
    AH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8),
                                                reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                                                reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    AH_CALL(Gte_PrimDepths4_10)(prim);
    prim[4] = prim[5] = prim[6] = 0x80;
    o = Cur();
    AH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(o + 0x34)), static_cast<unsigned long>(Long(o + 0x38)), 1, 0x48);
    AH_CALL(Gte_PopMatrix)();
}

// ===========================================================================
// Area 121: an effect-kind-0x18 state and an object trigger
// ===========================================================================

// original 0x41C790 (EffectKind18_States entry 70, 0x654184): with Cond_ByteFE
// 1 or 2 and Draw_PassFlags bit 2, a draw-mode primitive (Gpu_SetDrawMode(prim,
// 0, 1, 0x95, 0)) committed to slot 7, then an opaque POLY_G4 across the top of
// the screen - (0, 0), (320, 0), (0, h), (320, h), h the byte
// Area121_BandHeights[Cond_ByteFE] (0x624720) - coloured (0x48, 0x38, 0xFF)
// along the top edge and white along the bottom; Gfx_CommitPrim(7, 0x44).
extern "C" void __cdecl Area121_DrawTopBand(void) {
    const unsigned char band = Cond_ByteFE;
    if (band == 0 || band > 2) return;
    if ((Draw_PassFlags & 4) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    AH_CALL(Gfx_CommitPrim)(7, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 0);
    const float width = 320.0f;
    SetLong(prim + 8, 0);
    SetLong(prim + 0xC, 0);
    std::memcpy(prim + 0x18, &width, 4);
    SetLong(prim + 0x1C, 0);
    SetLong(prim + 0x28, 0);
    const float height = static_cast<float>(At(at::kBandHeights + Cond_ByteFE)[0]);
    std::memcpy(prim + 0x38, &width, 4);
    std::memcpy(prim + 0x2C, &height, 4);
    std::memcpy(prim + 0x3C, &height, 4);
    prim[0x14] = prim[4] = 0x48;
    prim[0x15] = prim[5] = 0x38;
    prim[0x16] = prim[6] = 0xFF;
    prim[0x36] = prim[0x26] = prim[0x35] = prim[0x25] = prim[0x34] = prim[0x24] = 0xFF;
    AH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x41C870 (Field_ObjectTriggers id 38, 0x662EB4; (object, flags)
// ignored): ScriptFlags_Set40; the tail kind 0x9039F3 = 0x2C (0x56DE50, the
// world map's field hook runner), 0x9039F4 = 0, 0x9039F5 = 0xE; al 0.
extern "C" unsigned char __cdecl Area121_Trigger38(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    At(at::kTailKind)[0] = 0x2C;
    At(at::kTailState)[0] = 0;
    At(at::kTailArg)[0] = 0xE;
    return 0;
}

// ===========================================================================

void AreaW3b_Inject() {
    if (bof3::WantsShadow("area_w3b")) area_w3b::SelfTest();
    BOF3_INJECT(Area120_ChoiceVar3A);
    BOF3_INJECT(Area120_ChoiceVar3B);
    BOF3_INJECT(Area120_ChoiceStartRun13);
    BOF3_INJECT(Area121_CameraRun);
    BOF3_INJECT(Area121_CameraFar);
    BOF3_INJECT(Area121_CameraApproach);
    BOF3_INJECT(Area121_StartRun4);
    BOF3_INJECT(Area121_SpawnEffectBA);
    BOF3_INJECT(Area121_PlaceMessage);
    BOF3_INJECT(Area121_PlateRun);
    BOF3_INJECT(Area121_PlateStart);
    BOF3_INJECT(Area121_PlateShow);
    BOF3_INJECT(Area121_PlateGrow);
    BOF3_INJECT(Area121_PlateHold);
    BOF3_INJECT(Area121_PlateShrink);
    BOF3_INJECT(Area121_HudRun);
    BOF3_INJECT(Area121_HudFrame);
    BOF3_INJECT(Area121_FrameStep);
    BOF3_INJECT(Area121_FrameSlideIn);
    BOF3_INJECT(Area121_FrameHold);
    BOF3_INJECT(Area121_FrameSlideOut);
    BOF3_INJECT(Area121_BoxStep);
    BOF3_INJECT(Area121_BoxSlideIn);
    BOF3_INJECT(Area121_BoxHold);
    BOF3_INJECT(Area121_BoxSlideOut);
    BOF3_INJECT(Area121_DrawFrame);
    BOF3_INJECT(Area121_DrawSprite);
    BOF3_INJECT(Area121_DrawHud);
    BOF3_INJECT(Area121_Record8Run);
    BOF3_INJECT(Area121_Record8Place);
    BOF3_INJECT(Area121_Record4Run);
    BOF3_INJECT(Area121_Record4MarkCell);
    BOF3_INJECT(Area121_DrawDrift);
    BOF3_INJECT(Area121_LeaderRun);
    BOF3_INJECT(Area121_LeaderControl);
    BOF3_INJECT(Area121_PushObject);
    BOF3_INJECT(Area121_StepAround);
    BOF3_INJECT(Area121_DirectionTo);
    BOF3_INJECT(Area121_StepOffObject);
    BOF3_INJECT(Area121_LeaderStep);
    BOF3_INJECT(Area121_MenuButton);
    BOF3_INJECT(Area121_Request4Button);
    BOF3_INJECT(Area121_TurnInput);
    BOF3_INJECT(Area121_Kind5CRun);
    BOF3_INJECT(Area121_Kind5CStart);
    BOF3_INJECT(Area121_Kind5CFollow);
    BOF3_INJECT(Area121_GaugeSprite);
    BOF3_INJECT(Area121_Kind5CFace);
    BOF3_INJECT(Area121_Kind5CTurnStep);
    BOF3_INJECT(Area121_Kind5CTurn);
    BOF3_INJECT(Area121_RingWait);
    BOF3_INJECT(Area121_RingRise);
    BOF3_INJECT(Area121_DrawTopBand);
    BOF3_INJECT(Area121_Trigger38);
}

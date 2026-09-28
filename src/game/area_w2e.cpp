// World 2's areas 104..106: the code of the PSX's BIN/WORLD02/AREA104..106.EMI
// compiled into the exe at 0x4146C0..0x4168DF - 52 functions (the band's 53
// less WorldMapHud_BoxWait 0x414BB0, round eight's, which lies in area 104's
// block), each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). Round ten group AR2E;
// docs/area_w2e.md has the areas one section each.
//
// Area 104 is WorldMap_Records record 6: eighteen of its functions are area
// 87's world-map code (docs/area_w2b.md section 4, itself area 45's and area
// 16's) instruction for instruction over area 104's tables - the plate's
// states 1..4, the HUD's frame and box machines, the three draws - with no
// constant of the copies' differing (no plate bank: its plate state 0 is
// 0x41ACD0, outside the band; the label cell is 0x803580 as areas 87's and
// 88's). Its place hook is its own (one message), and record 6 has no +4, +8
// or +0x14. The rest of area 104 is its own: a leader controller the engine
// runs as leader state 12 in area 104 (0x52FE90 jumps here when
// Game_AreaNumber is 0x68, to area 121's copy 0x41B9D0 otherwise) - walk,
// turn, step, a charge held on a button - with two functions that run only in
// area 121 (their first test is Game_AreaNumber 0x79); effect kind 0x5C (a
// companion that follows the leader and draws the charge's gauge; area 104's
// by the same area test in 0x462B60); effect kind 0x6A (a countdown drawn with
// a panel and a pulsing marker, which sends the party to area 0x79 when it
// runs out); mode-tail kind 40; a 4-bit minimap of the area's cells built at
// entry when key item 0xA is held; object trigger 36. Area 105: a tail, a
// step hook and an init. Area 106: three handlers placing an object by story
// flags, a tail that drops a party member in and moves the flags, a step hook
// that arms it on five cells.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table (the .data state tables, Effect_Objects by
// a slot Effect_FindFree answered "none" for, Sprite_ObjectsExtra by an object
// index of none, the upload queue past its twenty slots, the gauge's two-entry
// stack table, Area_Descriptors past its 200), ours aborts loudly (the
// owner's rule for an unchecked index, round9 doc section 6; no route reaches
// one). Every call goes through the harness (AH_CALL / AH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/area_w2e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2e::at;
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
unsigned char& B(U address) { return *At(address); }

// A state handler read from a .data table in place, as the originals' `call /
// jmp [index * 4 + table]`: the index is not checked there. Ours aborts past
// the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// An effect record by a slot Effect_FindFree answered: the originals do not
// test it (0xFF, "none", indexes far past the twenty; a signed slot below 0).
unsigned char* EffectRecord(const char* who, std::int32_t slot) {
    if (slot < 0 || slot >= static_cast<std::int32_t>(at::kEffectCount))
        bof3::Fatal("%s: effect slot %d is past Effect_Objects' 20 records", who, static_cast<int>(slot));
    return At(at::kEffects + static_cast<U>(slot) * at::kEffectStride);
}

// The group's own functions another of them calls directly (by address, so
// that the fuzz's recorder stands in and each is tested alone).
constexpr U kLeaderIdle = 0x415040;
constexpr U kObjectAhead121 = 0x4152B0;
constexpr U kTurnToFree = 0x4153F0;
constexpr U kStartOnObject121 = 0x415460;
constexpr U kStopMotion = 0x415640;
constexpr U kPoseByCharge = 0x415680;
constexpr U kLeaderCharge = 0x4156C0;
constexpr U kKind5CFollow = 0x415860;
constexpr U kDrawGauge = 0x415940;
constexpr U kKind5CTurn = 0x415A10;
constexpr U kKind5CTurnStep = 0x415A70;
constexpr U kKind5CSpin = 0x415B40;
constexpr U kDrawPanel = 0x4161F0;
constexpr U kBuildMinimap = 0x416020;
constexpr U kMinimapShade = 0x4160E0;
constexpr U kArmTail36 = 0x4168C0;

using AnswerFn = unsigned char (__cdecl*)();
using TurnToFreeFn = unsigned char (__cdecl*)(U, U, U);
using Turn121Fn = unsigned char (__cdecl*)(U, U, U);
using GaugeFn = void (__cdecl*)(U, U);
using GaugeFrameFn = void (__cdecl*)(U, U, U);
using TurnStepFn = unsigned char (__cdecl*)(U, U);
using ShadeFn = unsigned char (__cdecl*)(U, U);
using PrimFn = void (__cdecl*)(unsigned char*);
using ValueFn = void (__cdecl*)(U);

// A world-map copy's own functions another of them calls directly.
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

void Set40() { AH_CALL(ScriptFlags_Set40)(); }

}  // namespace

// ===========================================================================
// Area 104: the init and the place hook
// ===========================================================================

// original 0x4146C0 (area 104's init, its descriptor 0x61BB38's +0x40; PSX
// 0x801F2E8C): Field_ScriptFlags |= 0x140; mode-tail kind 40 armed (0x9039F3
// = 0x28). Without key item 0xA: the tail's state 0x9039F4 = 0 and
// Cond_ByteFE + 1. With it: Area104_BuildMinimap, state 2.
extern "C" void __cdecl Area104_Init(void) {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x140);
    B(at::kTailKind) = 0x28;
    if (AH_CALL(KeyItem_Has)(0xA) == 0) {
        B(at::kTailState) = 0;
        B(at::kCondFE) = static_cast<unsigned char>(B(at::kCondFE) + 1);
        return;
    }
    area_harness::Phase(kBuildMinimap)();
    B(at::kTailState) = 2;
}

// original 0x414700 (WorldMap_FieldHooks 0x662DF0 entry 6; PSX 0x801F2F0C): a
// two-state machine on the s8 0x9039F4. 0: ScriptFlags_Set40,
// Msg_OpenScript(4), then 0x9039F4 (read again) + 1 and Field_Request = 2. 1:
// once Field_Request is not 2, ScriptFlags_Clear40 and 0x9039F3..0x9039F5
// zeroed. Any other state does nothing. (Areas 87's and 88's hooks name the
// place; area 104's opens the one message.)
extern "C" void __cdecl Area104_PlaceMessage(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state == 0) {
        Set40();
        AH_CALL(Msg_OpenScript)(4);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    if (state != 1 || Field_Request == 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    B(at::kTailArg) = 0;
}

// ===========================================================================
// Area 104: the world map's seventh copy (area 87's code, docs/area_w2b.md
// section 4 - the same bodies over area 104's tables)
// ===========================================================================

namespace {

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (B(at::kMapMode) != 0 && o[0xB] != 0) return true;
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

// The place plate (the record's +0). The cell ahead of the leader - the high
// words of (leader +9) * (+0xC) + (+0x34) and of the same with +0x10 / +0x38
// - to +0xC / +0x10 (sign-extended), the kind +0xB by AreaMap_ByteAt(x, z)
// asked up to three times: 0xA1 1, 0xA0 2, 0xAE 3, else Field_ScriptFlags2
// bit 12 4, else 0; then the plate state table by +1 (read after the calls;
// ours aborts past its five entries).
void PlateRun(const WorldMapTables& t) {
    const U steps = B(at::kLeaderSteps);
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
    if (B(at::kMapMode) == 2) Cur()[2] = 3;
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
    if (B(at::kMapMode) != 2) {
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
    if (B(at::kMapMode) == 0 && Field_Request != 2 && (Field_ScriptFlags & 0x100) == 0) {
        o[3] = 1;
        o = Cur();
    }
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// The dial frame at (x, y) (area 33's WorldMap_DrawFrame 0x404390,
// instruction for instruction), nothing unless Draw_PassFlags & 0x1B. A
// draw-mode primitive committed to slot 1; the dial (sprite 0) at (x, y); the
// cell under the leader (AreaMap_ByteAt of the high words of Field_Kind2X /
// Z); legend 1 at (x + 0x30, y) unless the cell is 0xA0 / 0xA1 / 0xAE or
// Field_ScriptFlags2 bit 12, its key from button word 0 over six entries;
// legend 2 at (x + 0x30, y + 0x10) unless the cell is 0xA0 / 0xA1, its key
// from word 6 over EIGHT entries (in area 104 the seventh and eighth are the
// first two dwords of Area104_LeaderStates, code pointers); legend 3 when
// Field_CellHasEvent or flag bit 12, or the party set is 0xC, or
// Field_ScriptFlags bit 14; the needle at (x + 0x18, y + 0x18).
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
    if (!third) third = (B(at::kPartySet) & 0x7F) == 0xC;
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
// of the copy's label dword, read after the sprites - 0x803580 itself for area
// 104, as for areas 87 and 88).
void DrawHud(const WorldMapTables& t, int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x, y, 4);
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(t.label_offset))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

}  // namespace

// original 0x414760 (WorldMap_Records[6] +0; area 87's 0x40FDE0; PSX
// 0x801F2FB0): PlateRun through Area104_PlateStates 0x61BB7C (entry 0
// 0x41ACD0, outside the band).
extern "C" void __cdecl Area104_PlateRun(void) { PlateRun(at::kWm104); }
// original 0x414840 (Area104_PlateStates 1; area 87's 0x40FF20): PlateShow
// over Area104_PlateAnims 0x61B4F0 (one entry, place 0x65, then a zero
// record).
extern "C" void __cdecl Area104_PlateShow(void) { PlateShow(at::kWm104); }
// original 0x414990 (Area104_PlateStates 2): PlateGrow.
extern "C" void __cdecl Area104_PlateGrow(void) { PlateGrow(); }
// original 0x4149E0 (Area104_PlateStates 3): PlateHold.
extern "C" void __cdecl Area104_PlateHold(void) { PlateHold(); }
// original 0x414A40 (Area104_PlateStates 4): PlateShrink.
extern "C" void __cdecl Area104_PlateShrink(void) { PlateShrink(); }
// original 0x414A90 (record 6 +0xC; PSX 0x801F34E0): Area104_HudStates
// 0x61BB90 by +1 - WorldMapHud_Start (shared), Area104_HudFrame.
extern "C" void __cdecl Area104_HudRun(void) { StateEntry("Area104", "HudRun", at::kWm104.hud_states, 2, Cur()[1])(); }
// original 0x414AB0 (Area104_HudStates 1): `call 0x414AC0; jmp 0x414B90` -
// the frame's slide, then the region box's.
extern "C" void __cdecl Area104_HudFrame(void) {
    area_harness::Phase(at::kWm104.fn_frame_step)();
    area_harness::Phase(at::kWm104.fn_box_step)();
}
// original 0x414AC0: Area104_FrameStates 0x61BB98 by +2 - WorldMap_FrameWait
// (shared), _FrameSlideIn, _FrameHold, _FrameSlideOut. A tail jump; ours
// aborts past the four entries.
extern "C" void __cdecl Area104_FrameStep(void) { StateEntry("Area104", "FrameStep", at::kWm104.frame_states, 4, Cur()[2])(); }
// original 0x414AE0 (Area104_FrameStates 1): FrameSlideIn, then a tail jump
// to Area104_FrameHold 0x414B10.
extern "C" void __cdecl Area104_FrameSlideIn(void) { FrameSlideIn(at::kWm104); }
// original 0x414B10 (Area104_FrameStates 2): FrameHold, drawing through
// Area104_DrawFrame 0x414D30.
extern "C" void __cdecl Area104_FrameHold(void) { FrameHold(at::kWm104); }
// original 0x414B40 (Area104_FrameStates 3): FrameSlideOut.
extern "C" void __cdecl Area104_FrameSlideOut(void) { FrameSlideOut(at::kWm104); }
// original 0x414B90: Area104_BoxStates 0x61BBA8 by +3 - WorldMapHud_BoxWait
// (0x414BB0, round eight's, the next function), _BoxSlideIn, _BoxHold,
// _BoxSlideOut.
extern "C" void __cdecl Area104_BoxStep(void) { StateEntry("Area104", "BoxStep", at::kWm104.box_states, 4, Cur()[3])(); }
// original 0x414BF0 (Area104_BoxStates 1): BoxSlideIn, drawing through
// Area104_DrawHud 0x414FC0.
extern "C" void __cdecl Area104_BoxSlideIn(void) { BoxSlideIn(at::kWm104); }
// original 0x414C60 (Area104_BoxStates 2): BoxHold.
extern "C" void __cdecl Area104_BoxHold(void) { BoxHold(at::kWm104); }
// original 0x414CD0 (Area104_BoxStates 3): BoxSlideOut.
extern "C" void __cdecl Area104_BoxSlideOut(void) { BoxSlideOut(at::kWm104); }
// original 0x414D30: DrawFrame over Area104_Buttons 0x61BC10, drawing through
// Area104_DrawSprite 0x414F00.
extern "C" void __cdecl Area104_DrawFrame(int x, int y) { DrawFrame(at::kWm104, x, y); }
// original 0x414F00: DrawSprite over Area104_Sprites 0x61BBB8.
extern "C" void __cdecl Area104_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm104, x, y, index); }
// original 0x414FC0: DrawHud, the label's offset the low word of 0x803580.
extern "C" void __cdecl Area104_DrawHud(int x, int y) { DrawHud(at::kWm104, x, y); }

// ===========================================================================
// Area 104: the leader controller (leader state 12 in area 104)
// ===========================================================================

// original 0x415020 (reached by the tail jump at 0x52FE9A: leader state 12,
// 0x660918[12] = 0x52FE90, when Game_AreaNumber is 0x68): Area104_LeaderStates
// 0x61BC28 by the leader's +2 (called; ours aborts past its two entries - the
// next dwords are kind 0x5C's states), then a tail jump to
// Area104_LeaderCharge.
extern "C" void __cdecl Area104_LeaderRun(void) {
    StateEntry("Area104", "LeaderRun", at::kA104LeaderStates, at::kA104LeaderStateCount, Cur()[2])();
    area_harness::Phase(kLeaderCharge)();
}

// original 0x415040 (Area104_LeaderStates 0; and the tail jump of
// Area104_LeaderStep): the leader at rest.
//   - Area104_StartOnObject121 (area 121 only) answering 1, Field_ScriptFlags
//     bit 8, Field_ScriptFlags2 bit 6, Field_Request, or +0xA (the charge's
//     hold) not 0: nothing more.
//   - Field_InputHeld not 0: unless the actor state of Field_State +0x148 has
//     bit 5, Field_State +0x136 = 0.
//   - Field_LeaderCellEvent, area 121's menu-button test 0x41C0A0,
//     Field_LeaderTalkTest, area 121's hold test 0x41C0E0: the first to
//     answer ends it.
//   - The facing before the turn keys (0x41C110) is kept (b). When the turn
//     keys answered, or the charge button (word 0x903582) is held, and +0xB
//     is 0x40 (a full charge) - or neither answered and the button is not
//     held: +8 = b, Field_State +0x137 = 0, Area104_StopMotion, +9 = 0, +2 =
//     0 (the leader stops).
//   - Else, with the button held +8 = b; then a turn of more than two steps
//     from +8 to b is limited to one step either side of b (the s8 compares
//     over the 8-step wrap as the original makes them), +8 & 7.
//   - Field_LeaderStepTarget: 1 or 0xFF - +8 = b, Field_State +0x137 = 0,
//     +0x128 = 3, +9 = 0, +2 = 0; another non-zero - nothing; 0 - with
//     Field_LeaderPushObjects: Field_State +0x137 = 0, +2 = 0,
//     Area104_ObjectAhead121. Else Area104_PoseByCharge; with Field_State
//     +0x128 at 4 and +8 not b: +8 = b, +0x137 = 0, +2 = 0. Else
//     Field_JumpStart, Field_JumpCheckHeight, +9 - 1, Field_LeaderStepTick,
//     Field_State +0x137 = 1 and +2 = 1 (a step begins).
extern "C" void __cdecl Area104_LeaderIdle(void) {
    if (AH_AT(AnswerFn, kStartOnObject121)() != 0) return;
    if ((Field_ScriptFlags & 0x100) != 0) return;
    if ((B(at::kScriptFlags2) & 0x40) != 0) return;
    if (Field_Request != 0) return;
    if (Cur()[0xA] != 0) return;
    if (Word(At(at::kFieldInputHeld)) != 0) {
        unsigned char* const state = Field_State;
        const U actor = state[0x148];
        if ((B(at::kActorStates + actor * at::kActorStride) & 0x20) == 0) state[0x136] = 0;
    }
    if (AH_CALL(Field_LeaderCellEvent)() != 0) return;
    if (AH_AT(AnswerFn, area_w2e::kMenuButton121)() != 0) return;
    if (AH_CALL(Field_LeaderTalkTest)() != 0) return;
    if (AH_AT(AnswerFn, area_w2e::kHoldButton121)() != 0) return;
    const unsigned char b = Cur()[8];
    const unsigned char turned = AH_AT(AnswerFn, area_w2e::kTurnKeys121)();
    unsigned char* o = Cur();
    const U button = Word(At(at::kButtonCharge));
    const U held = Word(At(at::kInputHeld));
    const bool pressed = (button & held & 0xFFFF) != 0;
    if ((turned == 0 && !pressed) || o[0xB] == 0x40) {
        o[8] = b;
        Field_State[0x137] = 0;
        area_harness::Phase(kStopMotion)();
        Cur()[9] = 0;
        Cur()[2] = 0;
        return;
    }
    if (pressed) {
        o[8] = b;
        o = Cur();
    }
    auto cl = static_cast<signed char>(o[8]);
    if (cl != static_cast<signed char>(b)) {
        auto al = static_cast<signed char>(b);
        std::int32_t d = static_cast<std::int32_t>(static_cast<signed char>(b)) - cl;
        if (d < 0) d = -d;
        if (d > 2) {
            if (static_cast<signed char>(b) >= cl) cl = static_cast<signed char>(cl + 8);
            else al = static_cast<signed char>(b + 8);
        }
        std::int32_t d2 = static_cast<std::int32_t>(al) - cl;
        if (d2 < 0) d2 = -d2;
        if (d2 == 2) {
            o[8] = static_cast<unsigned char>(al > cl ? b - 1 : b + 1);
            o = Cur();
        }
        o[8] = static_cast<unsigned char>(o[8] & 7);
    }
    const unsigned char target = AH_CALL(Field_LeaderStepTarget)();
    if (target == 1 || target == 0xFF) {
        Cur()[8] = b;
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
        area_harness::Phase(kObjectAhead121)();
        return;
    }
    area_harness::Phase(kPoseByCharge)();
    if (Field_State[0x128] == 4 && Cur()[8] != b) {
        Cur()[8] = b;
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

namespace {
// A field object by the index Sprite_ObjectAt answered: 0..0x1D Sprite_Objects,
// 0x1E.. Sprite_ObjectsExtra (four records; the original does not test the
// index, and "none", 0xFF, reads 0xE1 records past them - ours aborts).
unsigned char* ObjectByIndex(const char* who, unsigned index) {
    if (index < 0x1E) return At(at::kObjects + index * at::kObjectStride);
    if (index - 0x1E >= 4) bof3::Fatal("%s: object %u is past Sprite_ObjectsExtra's 4 records", who, index);
    return At(at::kObjectsExtra + (index - 0x1E) * at::kObjectStride);
}
}  // namespace

// original 0x4152B0 (called by Area104_LeaderIdle after Field_LeaderPushObjects):
// nothing unless Game_AreaNumber is 0x79 (area 121: never in area 104, whose
// controller runs only there - this is area 121's code, which area 121's copy
// also has). The cell a step ahead - +0x34 / +0x38 plus twice the
// Field_DirectionSteps pair of +8 - asked of Sprite_ObjectAt(x, z, 1). With the
// charge button held and the object facing (+8 & 7) the leader's +8: its +0x80
// |= 1, done. Else Area104_TurnToFree(x, z, the object) (the index dword with
// the entry's stale ecx above its byte); answering 1: Field_JumpStart, +9 - 1,
// Field_LeaderStepTick, Field_State +0x137 = 1, +2 = 1.
extern "C" void __cdecl Area104_ObjectAhead121(void) {
    if (Game_AreaNumber != 0x79) return;
    unsigned char* const o = Cur();
    const U dir = o[8];
    const U x = static_cast<U>(Long(o + 0x34)) + static_cast<U>(Long(At(at::kDirectionSteps + dir * 8))) * 2u;
    const U z = static_cast<U>(Long(o + 0x38)) + static_cast<U>(Long(At(at::kDirectionSteps + dir * 8 + 4))) * 2u;
    const unsigned char object = AH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
    const U button = Word(At(at::kButtonCharge));
    const U held = Word(At(at::kInputHeld));
    if ((button & held & 0xFFFF) != 0) {
        unsigned char* const target = ObjectByIndex("Area104_ObjectAhead121", object);
        if (Cur()[8] == (target[8] & 7)) {
            target[0x80] = static_cast<unsigned char>(target[0x80] | 1);
            return;
        }
    }
    if (AH_AT(TurnToFreeFn, kTurnToFree)(x, z, object) == 0) return;
    AH_CALL(Field_JumpStart)();
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Field_LeaderStepTick)();
    Field_State[0x137] = 1;
    Cur()[2] = 1;
}

// original 0x4153F0 (x, z, object) -> al (called by area 121's two functions
// above): an object of 0xFF (none) answers 0. Else +8 = 0x41BE10(x, z,
// object) (area 121's: the octant from the object toward (x, z)); then up to
// eight times: Field_LeaderStepTarget answering 0 and Sprite_ObjectAt(x, z, 0)
// answering none (0xFF) answer 1; else +8 = (+8 + 1) & 7 (Sprite_Current read
// again). Eight turns: 0.
extern "C" unsigned char __cdecl Area104_TurnToFree(U x, U z, U object) {
    if ((object & 0xFF) == 0xFF) return 0;
    Cur()[8] = AH_AT(Turn121Fn, area_w2e::kTurnToward121)(x, z, object);
    for (int turns = 0; turns < 8; ++turns) {
        if (AH_CALL(Field_LeaderStepTarget)() == 0 &&
            AH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0) == 0xFF)
            return 1;
        unsigned char* const o = Cur();
        o[8] = static_cast<unsigned char>((o[8] + 1) & 7);
    }
    return 0;
}

// original 0x415460 () -> al (called first by Area104_LeaderIdle): 0 unless
// Game_AreaNumber is 0x79 (area 121's code; never in area 104). The object at
// the leader's own +0x34 / +0x38 (Sprite_ObjectAt(x, z, 1)) handed to
// Area104_TurnToFree with the position read again; answering 1:
// Field_JumpStart, +9 - 1, Field_LeaderStepTick, Field_State +0x137 = 1, +2 =
// 1, al 1. Else al 0.
extern "C" unsigned char __cdecl Area104_StartOnObject121(void) {
    if (Game_AreaNumber != 0x79) return 0;
    const unsigned char object = AH_CALL(Sprite_ObjectAt)(Long(Cur() + 0x34), Long(Cur() + 0x38), 1);
    unsigned char* const o = Cur();
    if (AH_AT(TurnToFreeFn, kTurnToFree)(static_cast<U>(Long(o + 0x34)), static_cast<U>(Long(o + 0x38)), object) == 0) return 0;
    AH_CALL(Field_JumpStart)();
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Field_LeaderStepTick)();
    Field_State[0x137] = 1;
    Cur()[2] = 1;
    return 1;
}

// original 0x4154E0 (Area104_LeaderStates 1): a step. Field_State +0x137 = 1.
// While +9 is not 0: +9 - 1 and a tail jump to Field_LeaderStepTick. At 0:
// Field_Bit20Tick, Field_FloorDamage; with Field_InputFlags bit 0, the cell
// under the word +0x36 / +0x3A: 0xAF - Field_ChangeArea(the pending area word,
// 0x903860, 0x90384C, 0x905B88) (the area and flag dwords pushed with stale
// bits above the word and byte Field_ChangeArea reads), 0x904EE0 = 0,
// 0x937F98 = 0xC, done; 0xC0 (asked again) - Area_LinkAt(x, z). Then
// Field_EdgeBits + 1, and + 1 more facing 2 or 6; Area104_StopMotion;
// Scenario_ArriveHook(+0x34, +0x38) not 0: +2 = 0, done. Field_Bit80Tick.
// Unless Field_ScriptFlags bit 8 or Field_ScriptFlags2 bit 6, a held button
// of (word 0x903582 | word 0x903580 | 0xF000) in Field_InputHeld is a tail
// jump to Area104_LeaderIdle. Else Field_State +0x137 = 0, +2 = 0.
extern "C" void __cdecl Area104_LeaderStep(void) {
    Field_State[0x137] = 1;
    unsigned char* const c = Cur();
    if (c[9] != 0) {
        c[9] = static_cast<unsigned char>(c[9] - 1);
        AH_CALL(Field_LeaderStepTick)();
        return;
    }
    AH_CALL(Field_Bit20Tick)();
    AH_CALL(Field_FloorDamage)();
    if ((B(at::kFieldInputFlags) & 1) != 0) {
        const auto cell = [] { return AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(Cur() + 0x36)), static_cast<short>(Word(Cur() + 0x3A))); };
        if (cell() == 0xAF) {
            AH_CALL(Field_ChangeArea)(Word(At(at::kPendingArea)), Long(At(at::kPendingX)), Long(At(at::kPendingZ)), B(at::kPendingFlags));
            B(at::kCell904EE0) = 0;
            B(at::kCell937F98) = 0xC;
            return;
        }
        if (cell() == 0xC0) AH_CALL(Area_LinkAt)(Word(Cur() + 0x36), Word(Cur() + 0x3A));
    }
    SetWord(At(at::kEdgeBits), Word(At(at::kEdgeBits)) + 1u);
    const unsigned char facing = Cur()[8];
    if (facing == 2 || facing == 6) SetWord(At(at::kEdgeBits), Word(At(at::kEdgeBits)) + 1u);
    area_harness::Phase(kStopMotion)();
    if (AH_CALL(Scenario_ArriveHook)(Long(Cur() + 0x34), Long(Cur() + 0x38)) != 0) {
        Cur()[2] = 0;
        return;
    }
    AH_CALL(Field_Bit80Tick)();
    if ((Field_ScriptFlags & 0x100) == 0 && (B(at::kScriptFlags2) & 0x40) == 0) {
        const U keys = (Word(At(at::kButtonCharge)) | Word(At(at::kButtonMap0)) | 0xF000u) & 0xFFFF;
        if ((Word(At(at::kFieldInputHeld)) & keys) != 0) {
            area_harness::Phase(kLeaderIdle)();
            return;
        }
    }
    Field_State[0x137] = 0;
    Cur()[2] = 0;
}

// original 0x415640 (called by Area104_LeaderIdle / _LeaderStep, and by area
// 121's code at 0x41BADE and 0x41C020): Field_State +0x128 = 3; the dwords
// +0xC, +0x10, +0x14 = 0 (Sprite_Current read again for each);
// MoveScript_F3Divisor = 0, MoveScript_FAWord = 0.
extern "C" void __cdecl Area104_StopMotion(void) {
    Field_State[0x128] = 3;
    SetLong(Cur() + 0xC, 0);
    SetLong(Cur() + 0x10, 0);
    SetLong(Cur() + 0x14, 0);
    SetWord(At(at::kF3Divisor), 0);
    SetWord(At(at::kFAWord), 0);
}

// original 0x415680 (called by Area104_LeaderIdle, and by area 121's code at
// 0x41BBEE): Field_State +0x128 = 4 when the charge button (word 0x903582) is
// held and +0xB is below 0x40; else 3.
extern "C" void __cdecl Area104_PoseByCharge(void) {
    const bool held = (Word(At(at::kButtonCharge)) & Word(At(at::kInputHeld))) != 0;
    Field_State[0x128] = static_cast<unsigned char>(held && Cur()[0xB] < 0x40 ? 4 : 3);
}

// original 0x4156C0 (the tail of Area104_LeaderRun, and of area 121's
// controller by its jump at 0x41B9E2): the charge. +0xA (a hold after a full
// charge) counts down; while it is not 0, nothing more. With the charge button
// held: +0xB + 1 below 0x40; at 0x40, +0xA = 0x1E. Released with +0xB not 0:
// a drain d of 3 above 0x30, 2 above 0x20, else 1 - while stepping (+2 not 0)
// d >> 1, and on odd frames d & 1 more; standing, d. Then +0xB held to
// 0..0x40 as a signed byte.
extern "C" void __cdecl Area104_LeaderCharge(void) {
    unsigned char* c = Cur();
    if (c[0xA] != 0) {
        c[0xA] = static_cast<unsigned char>(c[0xA] - 1);
        c = Cur();
    }
    if (c[0xA] != 0) return;
    const bool held = (Word(At(at::kButtonCharge)) & Word(At(at::kInputHeld))) != 0;
    const unsigned char al = c[0xB];
    if (held) {
        if (al < 0x40) {
            c[0xB] = static_cast<unsigned char>(al + 1);
            c = Cur();
        }
        if (c[0xB] == 0x40) {
            c[0xA] = 0x1E;
            c = Cur();
        }
    } else if (al != 0) {
        const unsigned char d = al > 0x30 ? 3 : al > 0x20 ? 2 : 1;
        if (c[2] != 0) {
            c[0xB] = static_cast<unsigned char>(al - (d >> 1));
            if ((Frame_Counter & 1) != 0) Cur()[0xB] = static_cast<unsigned char>(Cur()[0xB] - (d & 1));
        } else {
            c[0xB] = static_cast<unsigned char>(al - d);
        }
        c = Cur();
    }
    const auto v = static_cast<signed char>(c[0xB]);
    if (v < 0) c[0xB] = 0;
    else if (v > 0x40) c[0xB] = 0x40;
    else c[0xB] = static_cast<unsigned char>(v);
}

// ===========================================================================
// Area 104: effect kind 0x5C (Effect_KindHandlers[0x5C] 0x462B60 jumps here
// when Game_AreaNumber is 0x68, to area 121's 0x41C190 otherwise)
// ===========================================================================

// original 0x415780: Area104_Kind5CStates 0x61BC30 by +1 - _Kind5CStart,
// _Kind5CTurn, _Kind5CSpin, _Kind5CRise and 0x41C5B0 (area 121's). A tail
// jump; ours aborts past the five entries.
extern "C" void __cdecl Area104_Kind5CRun(void) {
    StateEntry("Area104", "Kind5CRun", at::kA104Kind5CStates, at::kA104Kind5CStateCount, Cur()[1])();
}

// original 0x4157A0 (Area104_Kind5CStates 0): +8 = the leader's facing;
// Sprite_InitFromEntry(Area_Descriptors[Game_AreaNumber] +8's pointer + 8);
// the tint +0x5D..+0x5F = 0x80; +1 + 1; Area104_Kind5CFollow. Then (in area
// 0x68 only with key item 0xA; in any other area always) four more effects of
// kind 0x5C, each from Effect_FindFree (unchecked: "none" writes far past the
// twenty; ours aborts), +0 = 1, kind +5 = 0x5C, +1 = 3 (the rise), +0xA = its
// delay 0, 8, 0x10, 0x18.
extern "C" void __cdecl Area104_Kind5CStart(void) {
    Cur()[8] = B(at::kLeaderDir);
    const U area = Game_AreaNumber;
    if (area >= 200) bof3::Fatal("Area104_Kind5CStart: area %u is past Area_Descriptors' 200", static_cast<unsigned>(area));
    const U descriptor = static_cast<U>(Long(At(at::kDescriptors + area * 4)));
    AH_CALL(Sprite_InitFromEntry)(At(static_cast<U>(Long(At(descriptor + 8))) + at::kA104EntryPlus8));
    Cur()[0x5D] = 0x80;
    Cur()[0x5E] = 0x80;
    Cur()[0x5F] = 0x80;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    area_harness::Phase(kKind5CFollow)();
    if (Game_AreaNumber == 0x68 && AH_CALL(KeyItem_Has)(0xA) == 0) return;
    for (unsigned k = 0; k < 4; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        unsigned char* const e = EffectRecord("Area104_Kind5CStart", slot);
        e[0] = 1;
        e[5] = 0x5C;
        e[1] = 3;
        e[0xA] = static_cast<unsigned char>(k << 3);
    }
}

// original 0x415860 (kind 0x5C's frame, called by its states): +0x34 / +0x38
// = the leader's, the word +0x3E = the leader's (Sprite_Current read again for
// each); Sprite_UpdateScreenA. Then the charge's gauge, nothing unless
// Draw_PassFlags & 0x1B: with Field_StatusBits bit 6, the frame
// (0x41C350(0xDC, 0x10, 0), area 121's) and both bars full
// (Area104_DrawGauge(0x40, 0), (0x40, 1)); else with the leader's +0xA 0 (no
// hold) the frame 0, the back bar full and the front bar 0x40 - the leader's
// +0xB; else (holding a full charge) the frame 1 or 2 by Frame_Counter bit 1
// and the back bar only.
extern "C" void __cdecl Area104_Kind5CFollow(void) {
    SetLong(Cur() + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    SetWord(Cur() + 0x3E, Word(At(at::kLeaderHeight)));
    AH_CALL(Sprite_UpdateScreenA)();
    if ((Draw_PassFlags & 0x1B) == 0) return;
    const auto frame = AH_AT(GaugeFrameFn, area_w2e::kGaugeFrame121);
    const auto bar = AH_AT(GaugeFn, kDrawGauge);
    if ((B(at::kStatusBits) & 0x40) != 0) {
        frame(0xDC, 0x10, 0);
        bar(0x40, 0);
        bar(0x40, 1);
        return;
    }
    if (B(at::kLeaderA) == 0) {
        frame(0xDC, 0x10, 0);
        bar(0x40, 0);
        bar(static_cast<unsigned char>(0x40 - B(at::kLeaderB)), 1);
        return;
    }
    frame(0xDC, 0x10, (Frame_Counter & 2) != 0 ? 1 : 2);
    bar(0x40, 0);
}

// original 0x415940 (width, bar) (called by Area104_Kind5CFollow, and by area
// 121's code at 0x41C2E1..0x41C342): one textured quad at Gfx_PacketNext -
// Gpu_SetPolyFT4; x from 228.0 to 228.0 + (width & 0xFF), y 29.0 to 39.0; v
// 0xD8..0xE0; u and CLUT from a two-entry stack table by (bar & 0xFF) - bar 0
// u 0x98, CLUT 0x780B; bar 1 u 0x90, CLUT 0x780E - u to u + 4; tpage 0xF,
// colour 0x80 x 3; Gfx_CommitPrim(2, 0x48). A bar past 1 reads the caller's
// return address as the table in the original: ours aborts.
extern "C" void __cdecl Area104_DrawGauge(U width, U bar) {
    static const unsigned char kTable[4] = {0xB0, 0x98, 0xE0, 0x90};
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(prim);
    const U i = (bar & 0xFF) * 2;
    if (i >= 4) bof3::Fatal("Area104_DrawGauge: bar %u is past its 2-entry table", static_cast<unsigned>(bar & 0xFF));
    const unsigned char u = kTable[i + 1];
    SetWord(prim + 0x16, 0x7800u | static_cast<U>(kTable[i] >> 4));
    const float left = 228.0f;
    const float right = static_cast<float>(static_cast<std::int32_t>(width & 0xFF)) + 228.0f;
    const float top = 29.0f, bottom = 39.0f;
    std::memcpy(prim + 0x08, &left, 4);
    std::memcpy(prim + 0x28, &left, 4);
    std::memcpy(prim + 0x0C, &top, 4);
    std::memcpy(prim + 0x1C, &top, 4);
    std::memcpy(prim + 0x2C, &bottom, 4);
    std::memcpy(prim + 0x3C, &bottom, 4);
    std::memcpy(prim + 0x18, &right, 4);
    std::memcpy(prim + 0x38, &right, 4);
    prim[0x14] = u;
    prim[0x34] = u;
    prim[0x15] = 0xD8;
    prim[0x25] = 0xD8;
    SetWord(prim + 0x26, 0xF);
    prim[0x24] = static_cast<unsigned char>(u + 4);
    prim[0x44] = static_cast<unsigned char>(u + 4);
    prim[0x35] = 0xE0;
    prim[0x45] = 0xE0;
    prim[4] = prim[5] = prim[6] = 0x80;
    AH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x415A10 (Area104_Kind5CStates 1; and the tail of _Kind5CSpin):
// the companion's +8 against the leader's facing. The same: a tail jump to
// Area104_Kind5CFollow. Else Area104_Kind5CTurnStep(+8, the facing), then
// (answering 0) with +8 ^ 4; both 0: the leader's facing = +8 and the follow.
extern "C" void __cdecl Area104_Kind5CTurn(void) {
    const auto step = AH_AT(TurnStepFn, kKind5CTurnStep);
    if (Cur()[8] != B(at::kLeaderDir)) {
        if (step(Cur()[8], B(at::kLeaderDir)) != 0) return;
        if (step(static_cast<U>(Cur()[8] ^ 4), B(at::kLeaderDir)) != 0) return;
        B(at::kLeaderDir) = Cur()[8];
    }
    area_harness::Phase(kKind5CFollow)();
}

// original 0x415A70 (from, to) -> al (the low bytes): with the two more than
// two steps apart, the smaller (unsigned) + 8 (over the wrap); then still
// more than two apart, or equal: al 0. Else one step toward `to` - +0x14 =
// -0x40 and +8 - 1 when from is above, else +0x14 = 0x40 and +8 + 1 - +8 & 7
// and the leader's facing = +8, +9 = 8, Area104_Kind5CSpin, +1 = 2, al 1.
extern "C" unsigned char __cdecl Area104_Kind5CTurnStep(U from, U to) {
    auto a = static_cast<unsigned char>(from);
    auto b = static_cast<unsigned char>(to);
    std::int32_t d = static_cast<std::int32_t>(a) - static_cast<std::int32_t>(b);
    if (d < 0) d = -d;
    if (d > 2) {
        if (a >= b) b = static_cast<unsigned char>(b + 8);
        else a = static_cast<unsigned char>(a + 8);
    }
    std::int32_t d2 = static_cast<std::int32_t>(a) - static_cast<std::int32_t>(b);
    if (d2 < 0) d2 = -d2;
    if (d2 > 2 || a == b) return 0;
    if (a > b) {
        SetLong(Cur() + 0x14, -0x40);
        Cur()[8] = static_cast<unsigned char>(Cur()[8] - 1);
    } else {
        SetLong(Cur() + 0x14, 0x40);
        Cur()[8] = static_cast<unsigned char>(Cur()[8] + 1);
    }
    Cur()[8] = static_cast<unsigned char>(Cur()[8] & 7);
    unsigned char* const o = Cur();
    B(at::kLeaderDir) = o[8];
    o[9] = 8;
    area_harness::Phase(kKind5CSpin)();
    Cur()[1] = 2;
    return 1;
}

// original 0x415B40 (Area104_Kind5CStates 2; called by _Kind5CTurnStep): the
// dword +0x6C += +0x14, & 0xFFF; +9 - 1; at 0 +1 = 1 and a tail jump to
// Area104_Kind5CTurn, else to Area104_Kind5CFollow (Sprite_Current read again
// for each step).
extern "C" void __cdecl Area104_Kind5CSpin(void) {
    SetLong(Cur() + 0x6C, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x6C)) + static_cast<U>(Long(Cur() + 0x14))));
    SetLong(Cur() + 0x6C, Long(Cur() + 0x6C) & 0xFFF);
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] == 0) {
        o[1] = 1;
        area_harness::Phase(kKind5CTurn)();
        return;
    }
    area_harness::Phase(kKind5CFollow)();
}

// original 0x415B90 (Area104_Kind5CStates 3): the four the start spawned wait
// out +0xA; then +0x34 / +0x38 = the leader's, +0x3C = the leader's + 0x800000,
// 0x41C5B0 (area 121's), +1 + 1 (to state 4, 0x41C5B0 again by the table).
extern "C" void __cdecl Area104_Kind5CRise(void) {
    unsigned char* const c = Cur();
    if (c[0xA] != 0) {
        c[0xA] = static_cast<unsigned char>(c[0xA] - 1);
        return;
    }
    SetLong(c + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    const auto y = static_cast<std::int32_t>(static_cast<U>(Long(At(at::kLeaderY))) + 0x800000u);
    SetLong(Cur() + 0x3C, y);
    AH_AT(area_harness::Handler, area_w2e::kKind5CState4)();
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// ===========================================================================
// Area 104: mode-tail kind 40, effect kind 0x6A (the countdown), the minimap,
// object trigger 36
// ===========================================================================

// original 0x415BE0 (Field_ModeTailKinds 0x662CE8 entry 40, armed by the
// init): by the s8 0x9039F4 (0..3, anything else nothing), each only once
// Field_Request is 0.
//   0: Msg_OpenScript(1); 0x9039F4 (read again) + 1, Field_Request = 2.
//   1: Field_ChangeArea(0x79, 0x190000, 0x2D0000, 7); Field_ScriptFlags &=
//      ~0x140; 0x9039F3 = 0x9039F4 = 0.
//   2 (the init's with key item 0xA): story flag 0x5B clear - set it and
//      Msg_OpenScript(2); set - Msg_OpenScript(3); then + 1 and Field_Request
//      = 2.
//   3: Field_ScriptFlags &= ~0x100; an effect from Effect_FindFree (none:
//      skipped; the slot sign-extended, unchecked - ours aborts outside
//      0..19): +0 + 1, kind +5 = 0x6A, +9 = 0x19, +0xA = 0, the words +0x36 /
//      +0x3A the leader's cell words; Music_FadeOutStop(0xA), Music_Play(0x94,
//      8); 0x9039F3 = 0x9039F4 = 0.
extern "C" void __cdecl Area104_Tail40(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state < 0 || state > 3 || Field_Request != 0) return;
    switch (state) {
    case 0: {
        AH_CALL(Msg_OpenScript)(1);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    case 1:
        AH_CALL(Field_ChangeArea)(0x79, 0x190000, 0x2D0000, 7);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEBF);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 2: {
        if (AH_CALL(Flags_Test)(Bank(), 0x5B) == 0) {
            AH_CALL(Flags_Set)(Bank(), 0x5B);
            AH_CALL(Msg_OpenScript)(2);
        } else {
            AH_CALL(Msg_OpenScript)(3);
        }
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    default: {
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            unsigned char* const e = EffectRecord("Area104_Tail40", static_cast<signed char>(slot));
            const U z = Word(At(at::kLeaderCellWordZ));
            e[0] = static_cast<unsigned char>(e[0] + 1);
            const U x = Word(At(at::kLeaderCellWordX));
            e[5] = 0x6A;
            e[9] = 0x19;
            e[0xA] = 0;
            SetWord(e + 0x36, x);
            SetWord(e + 0x3A, z);
        }
        AH_CALL(Music_FadeOutStop)(0xA);
        AH_CALL(Music_Play)(0x94, 8);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    }
    }
}

// original 0x415D50 (Effect_KindHandlers 0x655350 entry 0x6A, 0x6554F8; the
// effect Area104_Tail40 spawns): the countdown.
//   - Field_Request 5: Field_ScriptFlags &= ~0x40, Effect_Release, and it
//     still draws.
//   - Field_Request 0 with the leader in state 12 (its +1 0xC): +0xA (frames)
//     counts down; at 0 +9 (seconds) counts down with +0xA = 0x1D; both out:
//     Field_ChangeArea(0x79, 0x190000, 0x2D0000, 7), Field_ScriptFlags &=
//     ~0x40, a tail jump to Effect_Release (no draw).
//   - The draw: Menu_DrawBox(0x7E, 0x2E, 0x48, 0x11, 0xF2, the byte 0x903A5A),
//     Menu_DrawOutline(0x80, 0x30, 0x44, 0xD, 2); a digit - Rand() & 3 held to
//     2 when Field_Request is 0, else 0 - plus (+0xA % 3) * 3, held to 0..9 as
//     a signed byte; Crt_sprintf(0x904BA0, the format 0x61BC74, +9, +0xA / 3,
//     the digit); Text_DrawFont12(0x84, 0x30, 2 on frames with bit 2 while +9
//     is below 10, else 0, 0x904BA0); Area104_DrawPanel; then a ring of 24
//     semi-transparent triangles (0x5A7570, POLY_F3) round the leader's
//     place on the panel - centre ((cos(0x200) * (dx - dz)) >> 12) + 0xFA,
//     ((cos(0x200) * (dx + dz)) >> 12) + 0xAF for dx = the leader's cell
//     word x - 0x25, dz = z - 0x2D; radius 8 - ((Frame_Counter >> 1) & 7);
//     red 0xFF - radius * 0x1E (a byte); each after Gpu_SetDrawMode(prim, 0,
//     0, 0x3E, 0) and Gfx_CommitPrim(2, 0xC) once, Gfx_CommitPrim(2, 0x2C).
extern "C" void __cdecl Area104_Kind6ACountdown(void) {
    const unsigned char request = Field_Request;
    if (request == 5) {
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFBF);
        AH_CALL(Effect_Release)();
    } else if (request == 0 && B(at::kLeaderState) == 0xC) {
        unsigned char* const c = Cur();
        if (c[0xA] != 0) {
            c[0xA] = static_cast<unsigned char>(c[0xA] - 1);
        } else if (c[9] != 0) {
            c[9] = static_cast<unsigned char>(c[9] - 1);
            Cur()[0xA] = 0x1D;
        } else {
            AH_CALL(Field_ChangeArea)(0x79, 0x190000, 0x2D0000, 7);
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFBF);
            AH_CALL(Effect_Release)();
            return;
        }
    }
    AH_CALL(Menu_DrawBox)(0x7E, 0x2E, 0x48, 0x11, 0xF2, B(at::kMenuColour));
    AH_CALL(Menu_DrawOutline)(0x80, 0x30, 0x44, 0xD, 2);
    signed char digit = 0;
    if (Field_Request == 0) {
        const auto r = static_cast<signed char>(AH_CALL(Rand)() & 3);
        digit = r <= 2 ? r : 2;
    }
    const unsigned char* const c = Cur();
    const std::int32_t frames = c[0xA];
    digit = static_cast<signed char>(digit + static_cast<signed char>((frames % 3) * 3));
    if (digit < 0) digit = 0;
    else if (digit > 9) digit = 9;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kCountdownText)), reinterpret_cast<const char*>(At(at::kCountdownFormat)),
                         static_cast<U>(c[9]), frames / 3, static_cast<std::int32_t>(digit));
    const bool blink = Cur()[9] < 10 && (Frame_Counter & 4) != 0;
    AH_CALL(Text_DrawFont12)(0x84, 0x30, blink ? 2 : 0, At(at::kCountdownText));
    area_harness::Phase(kDrawPanel)();

    // the ring
    const std::int32_t dx = S16(At(at::kLeaderCellWordX)) - 0x25;
    const std::int32_t dz = S16(At(at::kLeaderCellWordZ)) - 0x2D;
    const std::int32_t cx = static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Cos)(0x200)) * static_cast<U>(dx - dz)) >> 12;
    const std::int32_t sx = cx + 0xFA;
    const std::int32_t cz = static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Cos)(0x200)) * static_cast<U>(dz + dx)) >> 12;
    const std::int32_t sy = cz + 0xAF;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x3E, 0);
    AH_CALL(Gfx_CommitPrim)(2, 0xC);
    const std::int32_t radius = 8 - static_cast<std::int32_t>((Frame_Counter >> 1) & 7);
    const float fx = static_cast<float>(sx);
    const float fy = static_cast<float>(sy);
    const auto red = static_cast<unsigned char>(0xFF - static_cast<unsigned char>(radius * 0x1E));
    const auto f3 = AH_AT(PrimFn, area_w2e::kSetPolyF3);
    for (std::int32_t angle = 0; angle < 0xFF0;) {
        unsigned char* const prim = Gfx_PacketNext;
        f3(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &fx, 4);
        std::memcpy(prim + 0xC, &fy, 4);
        std::int32_t v = (static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Cos)(angle)) * static_cast<U>(radius)) >> 12) + sx;
        float f = static_cast<float>(v);
        std::memcpy(prim + 0x14, &f, 4);
        v = (static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Sin)(angle)) * static_cast<U>(radius)) >> 12) + sy;
        f = static_cast<float>(v);
        std::memcpy(prim + 0x18, &f, 4);
        angle += 0xAA;
        v = (static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Cos)(angle)) * static_cast<U>(radius)) >> 12) + sx;
        f = static_cast<float>(v);
        std::memcpy(prim + 0x20, &f, 4);
        v = (static_cast<std::int32_t>(static_cast<U>(AH_CALL(Math_Sin)(angle)) * static_cast<U>(radius)) >> 12) + sy;
        prim[4] = red;
        prim[5] = 0;
        prim[6] = 0;
        f = static_cast<float>(v);
        std::memcpy(prim + 0x24, &f, 4);
        AH_CALL(Gfx_CommitPrim)(2, 0x2C);
    }
}

// original 0x4161F0 (called by Area104_Kind6ACountdown): the countdown's
// panel. One semi-transparent textured quad at Gfx_PacketNext (Gpu_SetPolyFT4,
// Gpu_SetSemiTrans 1): (255, 134), (300, 179), (210, 179), (255, 224) as
// floats, uv (0, 0), (0x40, 0), (0, 0x40), (0x40, 0x40), CLUT 0x7BC6, tpage
// 0x1E, colour 0x80 x 3, Gfx_CommitPrim(2, 0x48). Then the four triangles of
// Area104_PanelTris 0x61BC44 (0x5A7570, semi-transparent, colour (0, 0, 0xB8),
// each vertex's x and y from the row's s16 words as floats, Gfx_CommitPrim(2,
// 0x2C)). Every eighth frame the live CLUT word 0x811446 = 0x109F (Frame_Counter
// bit 3) or 8, and Gfx_ClutStripDirty + 1.
extern "C" void __cdecl Area104_DrawPanel(void) {
    unsigned char* const quad = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(quad);
    AH_CALL(Gpu_SetSemiTrans)(quad, 1);
    quad[0x24] = quad[0x44] = quad[0x35] = quad[0x45] = 0x40;
    const auto put = [](unsigned char* p, U bits) { std::memcpy(p, &bits, 4); };
    put(quad + 0x08, 0x437F0000);   // 255.0
    put(quad + 0x38, 0x437F0000);
    put(quad + 0x1C, 0x43330000);   // 179.0
    put(quad + 0x2C, 0x43330000);
    SetWord(quad + 0x26, 0x1E);
    SetWord(quad + 0x16, 0x7BC6);
    quad[0x14] = quad[0x34] = quad[0x15] = quad[0x25] = 0;
    put(quad + 0x18, 0x43960000);   // 300.0
    put(quad + 0x28, 0x43520000);   // 210.0
    put(quad + 0x0C, 0x43060000);   // 134.0
    put(quad + 0x3C, 0x43600000);   // 224.0
    quad[4] = quad[5] = quad[6] = 0x80;
    AH_CALL(Gfx_CommitPrim)(2, 0x48);
    const auto f3 = AH_AT(PrimFn, area_w2e::kSetPolyF3);
    for (U row = at::kPanelTris; row < at::kPanelTrisEnd; row += 0xC) {
        unsigned char* const prim = Gfx_PacketNext;
        f3(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        static const unsigned kOffsets[6] = {0x08, 0x14, 0x20, 0x0C, 0x18, 0x24};
        for (unsigned w = 0; w < 6; ++w) {
            const float f = static_cast<float>(S16(At(row + w * 2)));
            if (w == 5) {
                prim[4] = 0;
                prim[5] = 0;
                prim[6] = 0xB8;
            }
            std::memcpy(prim + kOffsets[w], &f, 4);
        }
        AH_CALL(Gfx_CommitPrim)(2, 0x2C);
    }
    const U frame = Frame_Counter;
    if ((frame & 7) != 0) return;
    SetWord(At(at::kClutWords + 6), (frame & 8) != 0 ? 0x109Fu : 8u);
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
}

// original 0x416020 (called by Area104_Init with key item 0xA; PSX 0x801F6564):
// a 4-bit image of the area's cells - the words 0x14, 0x44 at
// Gfx_UnpackScratch, then for z 0xD..0x51 and x 5..0x53 by twos a byte of
// Area104_MinimapShade(x + 1, z) << 4 | Area104_MinimapShade(x, z) (69 rows of
// 40 bytes; the header says 0x44 rows) - queued for upload at (0x380, 0x100)
// in Gfx_UploadQueue (its count unchecked against the twenty slots: ours
// aborts), four words of the live CLUT strip at 0x811440 set, and
// Gfx_ClutStripDirty + 1.
extern "C" void __cdecl Area104_BuildMinimap(void) {
    SetWord(At(at::kUnpack), 0x14);
    SetWord(At(at::kUnpack + 2), 0x44);
    unsigned char* out = At(at::kUnpack + 4);
    const auto shade = AH_AT(ShadeFn, kMinimapShade);
    for (U z = 0xD; z < 0x52; ++z) {
        for (U x = 5; x < 0x55; x += 2) {
            const auto hi = static_cast<unsigned char>(shade(x + 1, z) << 4);
            *out++ = static_cast<unsigned char>(hi | shade(x, z));
        }
    }
    const unsigned slot = B(at::kUploadCount);
    if (slot >= at::kUploadSlots) bof3::Fatal("Area104_BuildMinimap: upload slot %u is past the queue's %u", slot, at::kUploadSlots);
    SetWord(At(at::kUploadX + slot * 2), 0x380);
    SetWord(At(at::kUploadY + slot * 2), 0x100);
    SetLong(At(at::kUploadRecord + slot * 4), static_cast<std::int32_t>(at::kUnpack));
    const auto dirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    B(at::kUploadCount) = static_cast<unsigned char>(slot + 1);
    SetWord(At(at::kClutWords + 0), 0xFE10);
    SetWord(At(at::kClutWords + 2), 0xDC00);
    SetWord(At(at::kClutWords + 4), 0x8908);
    SetWord(At(at::kClutWords + 6), 0x085F);
    Gfx_ClutStripDirty = dirty;
}

// original 0x4160E0 (x, z) -> al (called by Area104_BuildMinimap; PSX
// 0x801F66C8): the shade of AreaMap_ByteAt(x, z) - 0x00, 0xAF and 0xC0 are
// 0, 0x10 and 0x50 are 1, 0x40 is 2, any other 3 (a byte-indexed table of
// 0xC1 in the code, then a jump table of four).
extern "C" unsigned char __cdecl Area104_MinimapShade(U x, U z) {
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    switch (cell) {
    case 0x00: case 0xAF: case 0xC0: return 0;
    case 0x10: case 0x50: return 1;
    case 0x40: return 2;
    default: return 3;
    }
}

// original 0x416340 (Field_ObjectTriggers 0x662EAC, id 36; (object,
// 0x904030)): ScriptFlags_Set40; mode-tail kind 44 (engine code) with state
// 0 and argument 9; al 0.
extern "C" unsigned char __cdecl Area104_Trigger36(unsigned char*, unsigned char*) {
    Set40();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailArg) = 9;
    return 0;
}

// ===========================================================================
// Area 105
// ===========================================================================

// original 0x416360 (Field_ModeTailKinds entry 61, armed by the step hook): by
// the s8 0x9039F4 - 0: Msg_OpenScript(1), Field_Request = 2, the state 1; 1:
// once Field_Request is not 2, ScriptFlags_Clear40 and 0x9039F3 = 0x9039F4 =
// 0. Any other state nothing.
extern "C" void __cdecl Area105_Tail61(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state == 0) {
        AH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        B(at::kTailState) = 1;
        return;
    }
    if (state != 1 || Field_Request == 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// original 0x4163B0 (area 105's step hook, through Area_StepHook at 0x56E122;
// (x, z) 16.16): al 0 when the chapter (s8 Cond_ByteFA) is 0xA or more, or
// Cond_ByteFD is set. Else the leader's facing is stored at 0x903850; facing
// 0, 1 or 2, or the high word of z (s16) at or below 0x1D: al 0. Else
// ScriptFlags_Set40, mode-tail kind 61 armed with state 0, al 1.
extern "C" unsigned char __cdecl Area105_StepHook(U, U z) {
    if (static_cast<signed char>(B(at::kCondFA)) >= 0xA) return 0;
    if (B(at::kCondFD) != 0) return 0;
    const unsigned char facing = B(at::kLeaderDir);
    B(at::kFoundX) = facing;
    if (facing == 0 || facing == 1 || facing == 2) return 0;
    if (static_cast<std::int16_t>(z >> 16) <= 0x1D) return 0;
    Set40();
    B(at::kTailKind) = 0x3D;
    B(at::kTailState) = 0;
    return 1;
}

// original 0x416400 (area 105's init; PSX 0x801F32B8): when the party came
// from area 0x57 (the word 0x802290): story flag 0x4F set when Cond_ByteFD is
// 0; then (read again) cleared when it is 1.
extern "C" void __cdecl Area105_Init(void) {
    if (Word(At(at::kPrevArea)) != 0x57) return;
    if (B(at::kCondFD) == 0) AH_CALL(Flags_Set)(Bank(), 0x4F);
    if (B(at::kCondFD) == 1) AH_CALL(Flags_Clear)(Bank(), 0x4F);
}

// ===========================================================================
// Area 106
// ===========================================================================

// original 0x416440 (area 106's handler 0, its descriptor 0x61C670's +0x3C
// 0x61C664; PSX 0x801F38E8): Field_ActiveMember +0x80 bit 0 cleared; when the
// byte of MoveScript_EffectState 0x66972C by the leader's +0x89 is 0,
// Cond_ByteFE = 1 and Sprite_Current (read before) +0 = 0.
extern "C" void __cdecl Area106_Handler0(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kEffectStates + B(at::kLeader89)) != 0) return;
    unsigned char* const c = Cur();
    B(at::kCondFE) = 1;
    c[0] = 0;
}

// original 0x416480 (area 106's handler 1; PSX 0x801F3940): places the running
// object by story flags 0x51 and 0x4F and moves the script on.
//   0x51 set: (0x128000, 0xB8000); 0x4F set - the word +0x3E 0x850 and the
//     script word MoveScript_Object +0xA + 2; clear - 0x600 and + 4.
//   0x51 clear: 0x4F set - (0x128000, 0x238000), 0x850, the script not moved;
//     clear - (0x118000, 0x1F8000), 0x300, + 4.
// (Sprite_Current read again for each store.)
extern "C" void __cdecl Area106_PlaceByFlags(void) {
    if (AH_CALL(Flags_Test)(Bank(), 0x51) != 0) {
        SetLong(Cur() + 0x34, 0x128000);
        SetLong(Cur() + 0x38, 0xB8000);
        if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
            SetWord(Cur() + 0x3E, 0x850);
            move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 2u);
        } else {
            SetWord(Cur() + 0x3E, 0x600);
            move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 4u);
        }
        return;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        SetLong(Cur() + 0x34, 0x128000);
        SetLong(Cur() + 0x38, 0x238000);
        SetWord(Cur() + 0x3E, 0x850);
        return;
    }
    SetLong(Cur() + 0x34, 0x118000);
    SetLong(Cur() + 0x38, 0x1F8000);
    SetWord(Cur() + 0x3E, 0x300);
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 4u);
}

// original 0x416550 (area 106's handler 2; PSX 0x801F3A60): k = (story flag
// 0x52 set) | (flag 0x53 set) << 1; the running object to the cell pair k of
// Area106_Cells 0x61C6B4 ((x << 16) | 0x8000, the same in z); then flag 0x4F
// set - the word +0x3E 0x850 and the script word + 8; clear - 0x280 and the
// script word + 2k.
extern "C" void __cdecl Area106_PlaceByPair(void) {
    const unsigned b52 = AH_CALL(Flags_Test)(Bank(), 0x52) != 0 ? 1u : 0u;
    const unsigned b53 = AH_CALL(Flags_Test)(Bank(), 0x53) != 0 ? 2u : 0u;
    const unsigned k = b52 | b53;
    const unsigned char* const pair = At(at::kA106Cells + k * 2);
    SetLong(Cur() + 0x34, static_cast<std::int32_t>(static_cast<U>(pair[0]) << 16 | 0x8000u));
    SetLong(Cur() + 0x38, static_cast<std::int32_t>(static_cast<U>(pair[1]) << 16 | 0x8000u));
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        SetWord(Cur() + 0x3E, 0x850);
        move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 8u);
        return;
    }
    SetWord(Cur() + 0x3E, 0x280);
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + k * 2u);
}

// original 0x416600 (Field_ModeTailKinds entry 36, armed by
// Area106_ArmTail36): by the s8 0x9039F4 (a byte table of 0x11 then a jump
// table of seven), each case Party_DropIn(n), 0x9039F3 = 0x9039F4 = 0, then
// the story flags: 0 - DropIn(0), set 0x51; 2 - DropIn(1), clear 0x51; 0xA -
// DropIn(2), set 0x52, clear 0x53; 0xC - DropIn(3), clear 0x52, set 0x53; 0xE
// - DropIn(4), set both; 0x10 - DropIn(5), clear both. Any other state
// (negative, odd, above 0x10, 4..8) nothing.
extern "C" void __cdecl Area106_Tail36(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    unsigned entry;
    switch (state) {
    case 0x0: entry = 0; break;
    case 0x2: entry = 1; break;
    case 0xA: entry = 2; break;
    case 0xC: entry = 3; break;
    case 0xE: entry = 4; break;
    case 0x10: entry = 5; break;
    default: return;
    }
    AH_CALL(Party_DropIn)(entry);
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    switch (entry) {
    case 0: AH_CALL(Flags_Set)(Bank(), 0x51); break;
    case 1: AH_CALL(Flags_Clear)(Bank(), 0x51); break;
    case 2: AH_CALL(Flags_Set)(Bank(), 0x52); AH_CALL(Flags_Clear)(Bank(), 0x53); break;
    case 3: AH_CALL(Flags_Clear)(Bank(), 0x52); AH_CALL(Flags_Set)(Bank(), 0x53); break;
    case 4: AH_CALL(Flags_Set)(Bank(), 0x52); AH_CALL(Flags_Set)(Bank(), 0x53); break;
    default: AH_CALL(Flags_Clear)(Bank(), 0x52); AH_CALL(Flags_Clear)(Bank(), 0x53); break;
    }
}

// original 0x416770 (area 106's step hook, through Area_StepHook at 0x56E132;
// (x, z) 16.16): al 0 when Cond_ByteFD is set. Otherwise one cell arms tail
// kind 36 (Area106_ArmTail36) with the state, al 1:
//   flag 0x4F set: 0x51 clear - z exactly 0x248000 and x's high word 0x11..0x13
//     (u16): state 0; 0x51 set - z 0xA8000, x's high word 0x12..0x13: 2.
//   0x4F clear, 0x52 set: z 0x258000, x's high word 0x1A..0x1B: 0x10 with 0x53
//     set, else 0xC.
//   0x4F and 0x52 clear, 0x53 set: x exactly 0x268000, z's high word
//     0x24..0x25: 0xE; 0x53 clear: x 0x188000, z's high word 0x15..0x16: 0xA.
// Any other cell al 0.
extern "C" unsigned char __cdecl Area106_StepHook(U x, U z) {
    if (B(at::kCondFD) != 0) return 0;
    const auto arm = AH_AT(ValueFn, kArmTail36);
    const auto in = [](U word, U lo, U n) { return static_cast<std::uint16_t>((word >> 16) - lo) < n; };
    if (AH_CALL(Flags_Test)(Bank(), 0x4F) != 0) {
        if (AH_CALL(Flags_Test)(Bank(), 0x51) == 0) {
            if (z != 0x248000 || !in(x, 0x11, 3)) return 0;
            arm(0);
            return 1;
        }
        if (z != 0xA8000 || !in(x, 0x12, 2)) return 0;
        arm(2);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x52) != 0) {
        if (z != 0x258000 || !in(x, 0x1A, 2)) return 0;
        arm(AH_CALL(Flags_Test)(Bank(), 0x53) != 0 ? 0x10 : 0xC);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x53) != 0) {
        if (x != 0x268000 || !in(z, 0x24, 2)) return 0;
        arm(0xE);
        return 1;
    }
    if (x != 0x188000 || !in(z, 0x15, 2)) return 0;
    arm(0xA);
    return 1;
}

// original 0x4168C0 (state) (called by Area106_StepHook): ScriptFlags_Set40;
// mode-tail kind 36 armed with the state's low byte.
extern "C" void __cdecl Area106_ArmTail36(U state) {
    Set40();
    B(at::kTailKind) = 0x24;
    B(at::kTailState) = static_cast<unsigned char>(state);
}

// ===========================================================================

void AreaW2e_Inject() {
    if (bof3::WantsShadow("area_w2e")) area_w2e::SelfTest();
    BOF3_INJECT(Area104_Init);
    BOF3_INJECT(Area104_PlaceMessage);
    BOF3_INJECT(Area104_PlateRun);
    BOF3_INJECT(Area104_PlateShow);
    BOF3_INJECT(Area104_PlateGrow);
    BOF3_INJECT(Area104_PlateHold);
    BOF3_INJECT(Area104_PlateShrink);
    BOF3_INJECT(Area104_HudRun);
    BOF3_INJECT(Area104_HudFrame);
    BOF3_INJECT(Area104_FrameStep);
    BOF3_INJECT(Area104_FrameSlideIn);
    BOF3_INJECT(Area104_FrameHold);
    BOF3_INJECT(Area104_FrameSlideOut);
    BOF3_INJECT(Area104_BoxStep);
    BOF3_INJECT(Area104_BoxSlideIn);
    BOF3_INJECT(Area104_BoxHold);
    BOF3_INJECT(Area104_BoxSlideOut);
    BOF3_INJECT(Area104_DrawFrame);
    BOF3_INJECT(Area104_DrawSprite);
    BOF3_INJECT(Area104_DrawHud);
    BOF3_INJECT(Area104_LeaderRun);
    BOF3_INJECT(Area104_LeaderIdle);
    BOF3_INJECT(Area104_ObjectAhead121);
    BOF3_INJECT(Area104_TurnToFree);
    BOF3_INJECT(Area104_StartOnObject121);
    BOF3_INJECT(Area104_LeaderStep);
    BOF3_INJECT(Area104_StopMotion);
    BOF3_INJECT(Area104_PoseByCharge);
    BOF3_INJECT(Area104_LeaderCharge);
    BOF3_INJECT(Area104_Kind5CRun);
    BOF3_INJECT(Area104_Kind5CStart);
    BOF3_INJECT(Area104_Kind5CFollow);
    BOF3_INJECT(Area104_DrawGauge);
    BOF3_INJECT(Area104_Kind5CTurn);
    BOF3_INJECT(Area104_Kind5CTurnStep);
    BOF3_INJECT(Area104_Kind5CSpin);
    BOF3_INJECT(Area104_Kind5CRise);
    BOF3_INJECT(Area104_Tail40);
    BOF3_INJECT(Area104_Kind6ACountdown);
    BOF3_INJECT(Area104_DrawPanel);
    BOF3_INJECT(Area104_BuildMinimap);
    BOF3_INJECT(Area104_MinimapShade);
    BOF3_INJECT(Area104_Trigger36);
    BOF3_INJECT(Area105_Tail61);
    BOF3_INJECT(Area105_StepHook);
    BOF3_INJECT(Area105_Init);
    BOF3_INJECT(Area106_Handler0);
    BOF3_INJECT(Area106_PlaceByFlags);
    BOF3_INJECT(Area106_PlaceByPair);
    BOF3_INJECT(Area106_Tail36);
    BOF3_INJECT(Area106_StepHook);
    BOF3_INJECT(Area106_ArmTail36);
}

// World 4's areas 152..155, 166 and 167: the code of the PSX's
// BIN/WORLD04/AREA152..167.EMI compiled into the exe at 0x4249D0..0x42655C -
// 48 functions, each read to its last instruction with capstone (2026-09-28)
// and taken through the area harness (area_harness.h). Areas 156..165 have no
// code in the band. Round ten group AR4A; docs/area_w4a.md has the areas one
// section each.
//
// Area 152 is the world map's eleventh and last copy (WorldMap_Records record
// 10): area 87's code (docs/area_w2b.md section 4, itself area 45's)
// instruction for instruction over area 152's tables, but for the plate's
// animation bank (0x1D1) and a field hook of its own - area 121's short shape
// (no cell test, no name sets) opening the message of the place's row by the
// chapter byte, as area 87's does on a place cell. Its init clears six story
// flags; its block also holds 0x4253C0, the record +8 effect's state 0 that
// all ten world maps' record-8 tables name (every 1024 frames, up to three
// kind-0x16 effects at the leader's ground). Areas 153 and 154: one choice
// each (a byte pair by the answer into the focus object). Area 155: a choice
// that gives an item, and the choice seven areas share (arm tail kind 10).
// Area 166: a pair choice that also arms tail kind 10. Area 167: four choices
// that arm its tail kind 32, eleven handlers (heights and flags 0x48..0x4C by
// Cond_ByteFD), the tail kind itself (party drop-ins, area changes, two
// countdowns), an arrive hook, a one-switch cell hook, an init.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 152's .data state tables abort past the table,
// where the original would jump through whatever the next dwords hold, and
// 0x4253C0 aborts on an effect slot past Effect_Objects' twenty (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches any).
// The out-of-table reads that land in mapped memory are reproduced as the
// original makes them (docs/area_w4a.md section 8). Every call goes through
// the harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in
// for the callees.
#include "game/area_w4a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4a::at;
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
unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Bank() { return At(at::kStoryFlags); }
bool Test(unsigned index) { return AH_CALL(Flags_Test)(Bank(), index) != 0; }
void Set(unsigned index) { AH_CALL(Flags_Set)(Bank(), index); }
void Clear(unsigned index) { AH_CALL(Flags_Clear)(Bank(), index); }
void SetMessage(unsigned v) { SetWord(At(at::kMessage), v); }
std::int32_t Answer() { return static_cast<signed char>(B(at::kChoice)); }
unsigned char* Focus() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kFocusObject))))); }
// MoveScript_Object's script word +0xA, moved by n (the op's position).
void ScriptSkip(unsigned n) { SetWord(MoveScript_Object + 0xA, Word(MoveScript_Object + 0xA) + n); }
void ArmTail(unsigned kind, unsigned state) {
    B(at::kTailKind) = static_cast<unsigned char>(kind);
    B(at::kTailState) = static_cast<unsigned char>(state);
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

}  // namespace

// ===========================================================================
// Area 152: its init, and the world map's eleventh copy (WorldMap_Records
// record 10; area 87's code, docs/area_w2b.md section 4; area 45's in
// docs/area_w1b.md section 5; area 33's in docs/worldmap_area.md sections 1..4)
// ===========================================================================

// original 0x4249D0 (area 152 +0x40, the init; PSX 0x801F2C38): story flags
// 0x75, 0x76 and 0x8B..0x8E cleared, in that order.
extern "C" void __cdecl Area152_Init(void) {
    Clear(0x75);
    Clear(0x76);
    Clear(0x8B);
    Clear(0x8C);
    Clear(0x8D);
    Clear(0x8E);
}

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

// The field hook (WorldMap_FieldHooks entry 10; area 152's own, 0x87 bytes
// where area 87's is 0x174 and area 121's 0x74): a two-state machine on the
// s8 0x9039F4.
//   0: ScriptFlags_Set40; the row of the place 0x937F82 among the copy's
//      place rows (searched in bounds; the row count, 3, when none); the
//      message word of row * 16 + (s8) Cond_ByteFA, read from the rows with
//      neither checked (row 3 is the plate state table; a negative chapter
//      reads before the rows); Msg_OpenScript. Then 0x9039F4 (read again
//      after the call) + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. Area 87's does the row part only on a 0xA1
// cell (else it names the cell's items); area 121's opens its place's index
// + 1.
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
    std::int32_t row = 0;
    for (U a = t.place_rows; a < t.place_rows_end; a += 0x20, ++row)
        if (Word(At(a)) == place) break;
    const std::int32_t index = (row << 4) + Cond_ByteFA;
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(t.place_rows + static_cast<U>(index * 2)))));
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
// area 152, as for areas 87, 88 and 121).
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

// original 0x424A30 (WorldMap_FieldHooks 0x662DF0 entry 10; PSX hook 0x801F2CA8):
// PlaceMessage over area 152's three place rows 0x637344.
extern "C" void __cdecl Area152_PlaceMessage(void) { PlaceMessage(at::kWm152); }
// original 0x424AC0 (WorldMap_Records[10] +0; area 87's 0x40FDE0; PSX
// 0x801F2DB0): PlateRun through Area152_PlateStates 0x6373A4.
extern "C" void __cdecl Area152_PlateRun(void) { PlateRun(at::kWm152); }
// original 0x424BA0 (Area152_PlateStates 0, and area 151's plate state 0 - its
// table 0x6371A4 names this body; area 87's 0x40FEC0): PlateStart with bank
// 0x1D1 (area 87's 0x156).
extern "C" void __cdecl Area152_PlateStart(void) { PlateStart(at::kWm152); }
// original 0x424C00 (Area152_PlateStates 1; area 87's 0x40FF20): PlateShow
// over Area152_PlateAnims 0x637288 (three entries).
extern "C" void __cdecl Area152_PlateShow(void) { PlateShow(at::kWm152); }
// original 0x424D50 (Area152_PlateStates 2; area 87's 0x410070): PlateGrow.
extern "C" void __cdecl Area152_PlateGrow(void) { PlateGrow(); }
// original 0x424DA0 (Area152_PlateStates 3; area 87's 0x4100C0): PlateHold.
extern "C" void __cdecl Area152_PlateHold(void) { PlateHold(); }
// original 0x424E00 (Area152_PlateStates 4; area 87's 0x410120): PlateShrink.
extern "C" void __cdecl Area152_PlateShrink(void) { PlateShrink(); }
// original 0x424E50 (record 10 +0xC; area 87's 0x410170; PSX 0x801F32E0):
// Area152_HudStates 0x6373B8 by +1 - WorldMapHud_Start (shared),
// Area152_HudFrame.
extern "C" void __cdecl Area152_HudRun(void) { StateEntry("Area152", "HudRun", at::kWm152.hud_states, 2, Cur()[1])(); }
// original 0x424E70 (Area152_HudStates 1): `call 0x424E80; jmp 0x424F50` - the
// frame's slide, then the region box's.
extern "C" void __cdecl Area152_HudFrame(void) {
    area_harness::Phase(at::kWm152.fn_frame_step)();
    area_harness::Phase(at::kWm152.fn_box_step)();
}
// original 0x424E80 (called by Area152_HudFrame): Area152_FrameStates 0x6373C0
// by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area152_FrameStep(void) { StateEntry("Area152", "FrameStep", at::kWm152.frame_states, 4, Cur()[2])(); }
// original 0x424EA0 (Area152_FrameStates 1): FrameSlideIn, then a tail jump
// to Area152_FrameHold 0x424ED0.
extern "C" void __cdecl Area152_FrameSlideIn(void) { FrameSlideIn(at::kWm152); }
// original 0x424ED0 (Area152_FrameStates 2): FrameHold, drawing through
// Area152_DrawFrame 0x4250B0.
extern "C" void __cdecl Area152_FrameHold(void) { FrameHold(at::kWm152); }
// original 0x424F00 (Area152_FrameStates 3): FrameSlideOut.
extern "C" void __cdecl Area152_FrameSlideOut(void) { FrameSlideOut(at::kWm152); }
// original 0x424F50 (called by Area152_HudFrame): Area152_BoxStates 0x6373D0
// by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl Area152_BoxStep(void) { StateEntry("Area152", "BoxStep", at::kWm152.box_states, 4, Cur()[3])(); }
// original 0x424F70 (Area152_BoxStates 1): BoxSlideIn, drawing through
// Area152_DrawHud 0x425340.
extern "C" void __cdecl Area152_BoxSlideIn(void) { BoxSlideIn(at::kWm152); }
// original 0x424FE0 (Area152_BoxStates 2): BoxHold.
extern "C" void __cdecl Area152_BoxHold(void) { BoxHold(at::kWm152); }
// original 0x425050 (Area152_BoxStates 3): BoxSlideOut.
extern "C" void __cdecl Area152_BoxSlideOut(void) { BoxSlideOut(at::kWm152); }
// original 0x4250B0 (called by the frame states): DrawFrame over
// Area152_Buttons 0x637438, drawing through Area152_DrawSprite 0x425280.
extern "C" void __cdecl Area152_DrawFrame(int x, int y) { DrawFrame(at::kWm152, x, y); }
// original 0x425280: DrawSprite over Area152_Sprites 0x6373E0.
extern "C" void __cdecl Area152_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm152, x, y, index); }
// original 0x425340 (called by the box states): DrawHud, the label's offset
// the low word of the dword 0x803580.
extern "C" void __cdecl Area152_DrawHud(int x, int y) { DrawHud(at::kWm152, x, y); }
// original 0x4253A0 (record 10 +8; PSX 0x801F4038): Area152_Record8States
// 0x637450 by +1 - Area152_Record8Spawn, _Record8Place, Area65_Record8Move.
extern "C" void __cdecl Area152_Record8Run(void) { StateEntry("Area152", "Record8Run", at::kWm152.record8_states, 3, Cur()[1])(); }

// original 0x4253C0 (Area152_Record8States 0, and the record-8 state table's
// entry 0 of every world map: areas 16, 33, 45, 65, 87, 88, 115, 121, 151 and
// 152's tables name this one body): Field_StatusBits bit 0 - Effect_Release,
// done. Else nothing unless Frame_Counter & 0x3FF is 0; then r = Rand() & 0x33
// (a byte) and n = r & 0xF (so 0..3): up to n times, Effect_FindFree - none
// (0xFF) ends it; the slot's record gets +0 = 1, kind +5 = 0x16, +1 = 1, +6 =
// the count so far, +8 = r >> 4, and the word +0x3E = AreaMap_Elevation(the
// leader's +0x34, +0x38, read before the stores) + 0x400. The slot is not
// checked against the twenty records (Effect_FindFree answers 0..19): ours
// aborts past them.
extern "C" void __cdecl Area152_Record8Spawn(void) {
    if ((Field_StatusBits & 1) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    if ((Frame_Counter & 0x3FF) != 0) return;
    const auto r = static_cast<unsigned char>(AH_CALL(Rand)() & 0x33);
    const std::int32_t n = r & 0xF;
    for (std::int32_t k = 0; k < n; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot == 0xFF) return;
        if (slot >= at::kEffectCount)
            bof3::Fatal("Area152_Record8Spawn: Effect_FindFree's slot %u is past Effect_Objects' %u", slot, at::kEffectCount);
        unsigned char* const e = At(at::kEffects + slot * at::kEffectStride);
        const long z = Long(At(at::kLeaderZ));
        const long x = Long(At(at::kLeaderX));
        e[0] = 1;
        e[5] = 0x16;
        e[1] = 1;
        e[6] = static_cast<unsigned char>(k);
        e[8] = static_cast<unsigned char>(r >> 4);
        SetWord(e + 0x3E, static_cast<unsigned>(AH_CALL(AreaMap_Elevation)(x, z)) + 0x400u);
    }
}

// original 0x425470 (Area152_Record8States 1): Record8Place over
// Area152_Directions 0x63745C and Area152_Record8Anims 0x63746C.
extern "C" void __cdecl Area152_Record8Place(void) { Record8Place(at::kWm152); }
// original 0x4255D0 (record 10 +4; PSX 0x801F448C): Area152_Record4States
// 0x637474 by +1 - Area152_Record4MarkCell, then Area45_Record4Tick 0x408990
// (shared).
extern "C" void __cdecl Area152_Record4Run(void) { StateEntry("Area152", "Record4Run", at::kWm152.record4_states, 2, Cur()[1])(); }
// original 0x4255F0 (Area152_Record4States 0): Record4MarkCell over the two
// cell records 0x637294.
extern "C" void __cdecl Area152_Record4MarkCell(void) { Record4MarkCell(at::kWm152); }
// original 0x4256B0 (record 10 +0x10; PSX 0x801F4628): DrawDrift over
// Area152_DriftUV 0x63747C (area 87's body: a square and the map-item grid).
extern "C" void __cdecl Area152_DrawDrift(void) { DrawDrift(at::kWm152); }

// ===========================================================================
// Areas 153 and 154: one choice each (their inits are 0x437CC0, a bare ret)
// ===========================================================================

namespace {
// The focus object's dwords +0x18 / +0x1C = the byte pair pairs[s8 answer]
// (zero-extended; the pointer and the answer read again for the second;
// unchecked), the message 0xFFFF between the two reads of the first.
void FocusPair(U pairs) {
    unsigned char* focus = Focus();
    const unsigned first = B(pairs + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Focus();
    SetLong(focus + 0x1C, B(pairs + static_cast<U>(Answer()) * 2u + 1));
}
}  // namespace

// original 0x425B20 (area 153 +0x34[0], a choice): FocusPair over
// Area153_ChoicePairs 0x63841C (Area90_ChoiceFocusPair's shape).
extern "C" void __cdecl Area153_ChoiceFocusPair(void) { FocusPair(at::kA153Pairs); }
// original 0x425B60 (area 154 +0x34[0]): FocusPair over Area154_ChoicePairs
// 0x638994.
extern "C" void __cdecl Area154_ChoiceFocusPair(void) { FocusPair(at::kA154Pairs); }

// ===========================================================================
// Area 155 (descriptor 0x638C80: a choice table only)
// ===========================================================================

// original 0x425BA0 (area 155 +0x34[0]): answer (byte) 2: Item_NamePtr(1,
// 0x4D)'s sixteen bytes to Text_Records row 0 (four dwords in order), then
// Inventory_Add(1, 0x4D, 1) answering (al) not 0: Sound_PlayEffect(0x106),
// message 0xB, bit 1 of the bank 0x903FF8 set, done. Any other answer, or the
// item not added: message 0xA and the same bit set.
extern "C" void __cdecl Area155_ChoiceGiveItem4D(void) {
    if (B(at::kChoice) == 2) {
        const unsigned char* const name = AH_CALL(Item_NamePtr)(1, 0x4D);
        unsigned char* const row = At(at::kTextRecords);
        for (U k = 0; k < 16; k += 4) SetLong(row + k, Long(name + k));
        if (AH_CALL(Inventory_Add)(1, 0x4D, 1) != 0) {
            AH_CALL(Sound_PlayEffect)(0x106);
            SetMessage(0xB);
            AH_CALL(Flags_Set)(At(at::kFlagBankF8), 1);
            return;
        }
    }
    SetMessage(0xA);
    AH_CALL(Flags_Set)(At(at::kFlagBankF8), 1);
}

// original 0x425C30 (area 155's block; the choice of seven areas: area 1
// +0x34[1], 23 [1], 40 [6], 41 [2], 143 [7], 166 [0], 168 [2]): the message
// 0xFFFF; answer (byte) 0: tail kind 0xA (0x56DB80), state 0, argument 0xFF.
extern "C" void __cdecl Area155_ChoiceArmTail10(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    ArmTail(0xA, 0);
    B(at::kTailArg) = 0xFF;
}

// ===========================================================================
// Area 166 (descriptor 0x63A758: a choice table only; choice 0 is
// Area155_ChoiceArmTail10)
// ===========================================================================

// original 0x425C60 (area 166 +0x34[1]): the message 0xFFFF; answer (byte) 3:
// ScriptFlags_Set40, tail kind 0xA, state 5, argument 0xFF. Then FocusPair
// over Area166_ChoicePairs 0x63A79C (the answer read again after the call).
extern "C" void __cdecl Area166_ChoiceFocusPair(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer == 3) {
        AH_CALL(ScriptFlags_Set40)();
        ArmTail(0xA, 5);
        B(at::kTailArg) = 0xFF;
    }
    const std::int32_t a = Answer();
    unsigned char* focus = Focus();
    SetLong(focus + 0x18, B(at::kA166Pairs + static_cast<U>(a) * 2u));
    focus = Focus();
    SetLong(focus + 0x1C, B(at::kA166Pairs + static_cast<U>(Answer()) * 2u + 1));
}

// ===========================================================================
// Area 167 (descriptor 0x63C550: choices 0x63C510, running on into the
// handlers 0x63C524 - choices 5..15 are handlers 0..10)
// ===========================================================================

// original 0x425CC0 (area 167 +0x34[0]): the message 0xFFFF; answer (byte) 2
// nothing more; else movement-script variable 5 = (answer != 0) + 1,
// ScriptFlags_Set40, variable 6 = 0x40, tail kind 0x20 (Area167_Tail32) at
// state 0x15.
extern "C" void __cdecl Area167_ChoiceTail32At15(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(at::kScriptVar5) = static_cast<unsigned char>((answer != 0 ? 1 : 0) + 1);
    AH_CALL(ScriptFlags_Set40)();
    B(at::kScriptVar6) = 0x40;
    ArmTail(0x20, 0x15);
}

// original 0x425D00 (+0x34[1]): the same with variable 5 = 2 or 0 (answer not
// 0 or 0) and state 0x14.
extern "C" void __cdecl Area167_ChoiceTail32At14Even(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(at::kScriptVar5) = static_cast<unsigned char>(answer != 0 ? 2 : 0);
    AH_CALL(ScriptFlags_Set40)();
    B(at::kScriptVar6) = 0x40;
    ArmTail(0x20, 0x14);
}

// original 0x425D40 (+0x34[2]): the same with variable 5 = 1 or 0 and state
// 0x14.
extern "C" void __cdecl Area167_ChoiceTail32At14(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(at::kScriptVar5) = static_cast<unsigned char>(answer != 0 ? 1 : 0);
    AH_CALL(ScriptFlags_Set40)();
    B(at::kScriptVar6) = 0x40;
    ArmTail(0x20, 0x14);
}

// original 0x425D80 (+0x34[3] and [4]): the message 0xFFFF; answer (byte) 0:
// ScriptFlags_Set40, variable 6 = 0x60, tail kind 0x20 at state 0xA.
extern "C" void __cdecl Area167_ChoiceTail32AtA(void) {
    const unsigned char answer = B(at::kChoice);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kScriptVar6) = 0x60;
    ArmTail(0x20, 0xA);
}

// original 0x425DB0 (handler 0 = choice 5; also area 77's handler 20 / choice
// 22 and area 148's handler 3 / choice 5; PSX 0x801F3F14): Cond_ByteFE = 2.
extern "C" void __cdecl Area167_SetByteFE2(void) { Cond_ByteFE = 2; }

// original 0x425DC0 (handler 2; PSX 0x801F3F58): Cond_ByteFD 2 - story flag
// 0x4C set: cleared, MoveScript_Object +3 = 5 and its script word +0xA =
// 0xFFFE, done; flag 0x4B clear: Sprite_Current's word +0x3E = 0xFDC0 and +0
// |= 0x40, done. Then (read again) 1 - flag 0x4C set: cleared, +3 = 4 and the
// word 0xFFFE, done; flag 0x4B set: +0x3E = 0x9C0, +0 |= 0x40, done. Else
// +0x3E = 0x3C0. Every pointer read again for each store.
extern "C" void __cdecl Area167_HeightBy4B4C(void) {
    if (Cond_ByteFD == 2) {
        if (Test(0x4C)) {
            Clear(0x4C);
            MoveScript_Object[3] = 5;
            SetWord(MoveScript_Object + 0xA, 0xFFFE);
            return;
        }
        if (!Test(0x4B)) {
            SetWord(Sprite_Current + 0x3E, 0xFDC0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    if (Cond_ByteFD == 1) {
        if (Test(0x4C)) {
            Clear(0x4C);
            MoveScript_Object[3] = 4;
            SetWord(MoveScript_Object + 0xA, 0xFFFE);
            return;
        }
        if (Test(0x4B)) {
            SetWord(Sprite_Current + 0x3E, 0x9C0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    SetWord(Sprite_Current + 0x3E, 0x3C0);
}

// original 0x425EA0 (handler 3; PSX 0x801F4080): Sprite_Current's word +0x3E
// 0x3C0: MoveScript_Object's script word + 8. Else +0 bit 6 cleared and
// story flag 0x4B toggled.
extern "C" void __cdecl Area167_Toggle4BUnlessLow(void) {
    unsigned char* const o = Sprite_Current;
    if (Word(o + 0x3E) == 0x3C0) {
        ScriptSkip(8);
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    AH_CALL(Flags_Toggle)(Bank(), 0x4B);
}

// original 0x425ED0 (handler 4; PSX 0x801F40F4): story flag 0x4B clear:
// MoveScript_Object's script word + 3.
extern "C" void __cdecl Area167_Skip3Unless4B(void) {
    if (!Test(0x4B)) ScriptSkip(3);
}

// original 0x425EF0 (handler 5; PSX 0x801F4140): Cond_ByteFD 2 - story flag
// 0x4A set: cleared, MoveScript_Object's script word = 0xFFFE, its +3 = 7
// when the byte 0x905E68 is 0 else 8, done; flag 0x48 clear: +0x3E = 0x9C0
// (flag 0x49 clear) or 0xFDC0 (set) and +0 |= 0x40, done. Then (read again) 1
// - flag 0x4A set: cleared, +3 = 7 and the word 0xFFFE, done; flag 0x49
// clear: +0x3E = 0x9C0, +0 |= 0x40, done. Else +0x3E = 0x3C0.
extern "C" void __cdecl Area167_HeightBy48To4A(void) {
    if (Cond_ByteFD == 2) {
        if (Test(0x4A)) {
            Clear(0x4A);
            SetWord(MoveScript_Object + 0xA, 0xFFFE);
            if (B(at::kByte905E68) == 0) MoveScript_Object[3] = 7;
            else MoveScript_Object[3] = 8;
            return;
        }
        if (!Test(0x48)) {
            const unsigned height = Test(0x49) ? 0xFDC0u : 0x9C0u;
            SetWord(Sprite_Current + 0x3E, height);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    if (Cond_ByteFD == 1) {
        if (Test(0x4A)) {
            Clear(0x4A);
            MoveScript_Object[3] = 7;
            SetWord(MoveScript_Object + 0xA, 0xFFFE);
            return;
        }
        if (!Test(0x49)) {
            SetWord(Sprite_Current + 0x3E, 0x9C0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    SetWord(Sprite_Current + 0x3E, 0x3C0);
}

// original 0x426010 (handler 6; PSX 0x801F42C4): Sprite_Current's word +0x3E
// 0x3C0: MoveScript_Object's script word + 8. Else +0 bit 6 cleared, and by
// Cond_ByteFD (read once): 1 - flag 0x48 set, 0x49 cleared; 2 - 0x48 cleared,
// 0x49 set; 3 - both cleared; else nothing more.
extern "C" void __cdecl Area167_Flags48By49(void) {
    unsigned char* const o = Sprite_Current;
    if (Word(o + 0x3E) == 0x3C0) {
        ScriptSkip(8);
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    const unsigned char chapter = Cond_ByteFD;
    if (chapter == 1) {
        Set(0x48);
        Clear(0x49);
    } else if (chapter == 2) {
        Clear(0x48);
        Set(0x49);
    } else if (chapter == 3) {
        Clear(0x48);
        Clear(0x49);
    }
}

// original 0x4260A0 (handler 7; PSX 0x801F43B8): Cond_ByteFD 2 - variable 5
// not 0: done; else MoveScript_Object's script word + 3. Then (read again)
// Cond_ByteFD 1: the script word + 3.
extern "C" void __cdecl Area167_Skip3ByChapter(void) {
    if (Cond_ByteFD == 2) {
        if (B(at::kScriptVar5) != 0) return;
        ScriptSkip(3);
    }
    if (Cond_ByteFD == 1) ScriptSkip(3);
}

// original 0x4260E0 (handler 10 = choice 15; PSX 0x801F4480): Cond_ByteFE =
// 0x10.
extern "C" void __cdecl Area167_SetByteFE10(void) { Cond_ByteFE = 0x10; }

// original 0x4260F0 (Field_ModeTailKinds[32] 0x662D68; armed by area 167's
// choices, arrive hook, cell hook and init): a switch on the s8 0x9039F4 -
// above 0x20 (unsigned) nothing - through the byte table 0x426440 in the code
// (read in place: 33 cases, 0..0xD) and a jump table of fourteen:
//   0: Party_DropIn(0), state 1.
//   1: variable 6 0x18: kind, state 0; flag 0x46 set; Field_ChangeArea(0x95,
//      0x80000, 0xC0000, 0x81).
//   5: Sound_PlayEffect(0x204), state 6, the countdown 0x9039F6 = 0xA.
//   6: the countdown - 1; at 0 ScriptFlags_Clear40, kind and state 0.
//   0xA: variable 6 0x64: Party_DropIn(5), state 0xB.
//   0xB: variable 6 0x74: Field_ChangeArea(0xA7, 0x130000, 0x9A0000, 0x87)
//      with Cond_ByteFD 2, else (0xA7, 0xA0000, 0x1E0000, 0x86); flag 0x4B
//      toggled, 0x4C set, state 0xC.
//   0xC: variable 6 0: Sound_PlayEffect(0x206) with Cond_ByteFD 0; variable 5
//      = 0, kind and state 0.
//   0x14: variable 6 0x44: Party_DropIn(2), state 0x18.
//   0x15: Party_DropIn(8), state 0x18.
//   0x18: variable 6 0x54: by variable 5 (read once) - 0: variable 6 = 0,
//      ScriptFlags_Clear40, Field_ChangeArea(0xA7, 0x1A8000, 0x560000, 5),
//      flags 0x48 and 0x49 cleared; 1: (0xA7, 0x200000, 0xF0000, 0x83), 0x48
//      set, 0x49 cleared; 2: (0xA7, 0x290000, 0x8B0000, 0x84), 0x48 cleared,
//      0x49 set. Then Sound_PlayEffect(0x205) with Cond_ByteFD 0, flag 0x4A
//      set, state 0xC.
//   0x1E: state 0x1F.
//   0x1F: MoveScript_WaitWordDA 0: Cond_ByteFE = 0x20, the countdown 0x1E,
//      state 0x20.
//   0x20: the countdown - 1; at 0 ScriptFlags_Clear40, flag 0x90 set,
//      Field_ChangeArea(0xA8, 0x150000, 0x40000, 7), Field_ScriptFlags2 bit 6
//      set (a byte or), kind and state 0.
// The other states (2..4, 7..9, 0xD..0x13, 0x16, 0x17, 0x19..0x1D) do nothing.
// Ours aborts on a case byte past the fourteen (the table is code bytes: it
// cannot happen but by a changed exe).
extern "C" void __cdecl Area167_Tail32(void) {
    const auto state = static_cast<std::int32_t>(static_cast<signed char>(B(at::kTailState)));
    if (static_cast<U>(state) > 0x20) return;
    const unsigned which = B(at::kTail32Index + static_cast<U>(state));
    switch (which) {
    case 0:
        AH_CALL(Party_DropIn)(0);
        B(at::kTailState) = 1;
        return;
    case 1:
        if (B(at::kScriptVar6) != 0x18) return;
        ArmTail(0, 0);
        Set(0x46);
        AH_CALL(Field_ChangeArea)(0x95, 0x80000, 0xC0000, 0x81);
        return;
    case 2:
        AH_CALL(Sound_PlayEffect)(0x204);
        B(at::kTailState) = 6;
        SetWord(At(at::kTailTimer), 0xA);
        return;
    case 3:
        SetWord(At(at::kTailTimer), Word(At(at::kTailTimer)) - 1u);
        if (Word(At(at::kTailTimer)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        ArmTail(0, 0);
        return;
    case 4:
        if (B(at::kScriptVar6) != 0x64) return;
        AH_CALL(Party_DropIn)(5);
        B(at::kTailState) = 0xB;
        return;
    case 5:
        if (B(at::kScriptVar6) != 0x74) return;
        if (Cond_ByteFD == 2) AH_CALL(Field_ChangeArea)(0xA7, 0x130000, 0x9A0000, 0x87);
        else AH_CALL(Field_ChangeArea)(0xA7, 0xA0000, 0x1E0000, 0x86);
        AH_CALL(Flags_Toggle)(Bank(), 0x4B);
        Set(0x4C);
        B(at::kTailState) = 0xC;
        return;
    case 6:
        if (B(at::kScriptVar6) != 0) return;
        if (Cond_ByteFD == 0) AH_CALL(Sound_PlayEffect)(0x206);
        B(at::kScriptVar5) = 0;
        ArmTail(0, 0);
        return;
    case 7:
        if (B(at::kScriptVar6) != 0x44) return;
        AH_CALL(Party_DropIn)(2);
        B(at::kTailState) = 0x18;
        return;
    case 8:
        AH_CALL(Party_DropIn)(8);
        B(at::kTailState) = 0x18;
        return;
    case 9: {
        if (B(at::kScriptVar6) != 0x54) return;
        const unsigned char way = B(at::kScriptVar5);
        if (way == 0) {
            B(at::kScriptVar6) = 0;
            AH_CALL(ScriptFlags_Clear40)();
            AH_CALL(Field_ChangeArea)(0xA7, 0x1A8000, 0x560000, 5);
            Clear(0x48);
            Clear(0x49);
        } else if (way == 1) {
            AH_CALL(Field_ChangeArea)(0xA7, 0x200000, 0xF0000, 0x83);
            Set(0x48);
            Clear(0x49);
        } else if (way == 2) {
            AH_CALL(Field_ChangeArea)(0xA7, 0x290000, 0x8B0000, 0x84);
            Clear(0x48);
            Set(0x49);
        }
        if (Cond_ByteFD == 0) AH_CALL(Sound_PlayEffect)(0x205);
        Set(0x4A);
        B(at::kTailState) = 0xC;
        return;
    }
    case 10:
        B(at::kTailState) = 0x1F;
        return;
    case 11:
        if (MoveScript_WaitWordDA != 0) return;
        Cond_ByteFE = 0x20;
        SetWord(At(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x20;
        return;
    case 12:
        SetWord(At(at::kTailTimer), Word(At(at::kTailTimer)) - 1u);
        if (Word(At(at::kTailTimer)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Set(0x90);
        AH_CALL(Field_ChangeArea)(0xA8, 0x150000, 0x40000, 7);
        B(0x905BA4) = static_cast<unsigned char>(B(0x905BA4) | 0x40);   // Field_ScriptFlags2's low byte
        ArmTail(0, 0);
        return;
    case 13:
        return;
    default:
        bof3::Fatal("Area167_Tail32: case byte %u (state %d) is past the fourteen-entry jump table 0x426408", which, state);
    }
}

// original 0x426470 (Area_ArriveHook's case for area 167): (x, z) 16.16; z's
// high word 0x66 and x's high word 0x14..0x16 (u16): ScriptFlags_Set40, tail
// kind 0x20 at state 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area167_ArriveHook(long x, long z) {
    if (static_cast<std::uint16_t>(static_cast<U>(z) >> 16) != 0x66) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(x) >> 16) - 0x14u) >= 3) return 0;
    AH_CALL(ScriptFlags_Set40)();
    ArmTail(0x20, 0);
    return 1;
}

// original 0x4264A0 (Area_CellHooks, area 167's): (x, z) as bytes; the
// switch records 0x63C594 (one) searched for x, z and a facing nibble (& 0xF)
// equal to the leader's whole facing byte +8 (read once, before the search):
// none al 0. Else its flag toggled in the story bank, ScriptFlags_Set40, tail
// kind 0x20 at state 5, al 1.
extern "C" unsigned char __cdecl Area167_CellHook(unsigned x, unsigned z) {
    const unsigned char facing = B(at::kLeaderDir);
    const auto bx = static_cast<unsigned char>(x);
    const auto bz = static_cast<unsigned char>(z);
    U found = 0;
    for (U a = at::kA167Switch; a < at::kA167SwitchEnd; a += 4, ++found) {
        const unsigned char* const r = At(a);
        if (r[0] == bx && r[1] == bz && (r[2] & 0xF) == facing) break;
    }
    if (found == 1) return 0;
    AH_CALL(Flags_Toggle)(Bank(), B(at::kA167Switch + 3 + found * 4));
    AH_CALL(ScriptFlags_Set40)();
    ArmTail(0x20, 5);
    return 1;
}

// original 0x426510 (area 167 +0x40, the init; PSX 0x801F49F0): with
// Cond_ByteFD 0, story flag 0x8F set and 0x90 clear: Camera_Distance = 0x980,
// tail kind 0x20 at state 0x1E.
extern "C" void __cdecl Area167_Init(void) {
    if (Cond_ByteFD != 0) return;
    if (!Test(0x8F)) return;
    if (Test(0x90)) return;
    SetWord(At(at::kCameraDistance), 0x980);
    ArmTail(0x20, 0x1E);
}

// ===========================================================================

void AreaW4a_Inject() {
    if (bof3::WantsShadow("area_w4a")) area_w4a::SelfTest();
    BOF3_INJECT(Area152_Init);
    BOF3_INJECT(Area152_PlaceMessage);
    BOF3_INJECT(Area152_PlateRun);
    BOF3_INJECT(Area152_PlateStart);
    BOF3_INJECT(Area152_PlateShow);
    BOF3_INJECT(Area152_PlateGrow);
    BOF3_INJECT(Area152_PlateHold);
    BOF3_INJECT(Area152_PlateShrink);
    BOF3_INJECT(Area152_HudRun);
    BOF3_INJECT(Area152_HudFrame);
    BOF3_INJECT(Area152_FrameStep);
    BOF3_INJECT(Area152_FrameSlideIn);
    BOF3_INJECT(Area152_FrameHold);
    BOF3_INJECT(Area152_FrameSlideOut);
    BOF3_INJECT(Area152_BoxStep);
    BOF3_INJECT(Area152_BoxSlideIn);
    BOF3_INJECT(Area152_BoxHold);
    BOF3_INJECT(Area152_BoxSlideOut);
    BOF3_INJECT(Area152_DrawFrame);
    BOF3_INJECT(Area152_DrawSprite);
    BOF3_INJECT(Area152_DrawHud);
    BOF3_INJECT(Area152_Record8Run);
    BOF3_INJECT(Area152_Record8Spawn);
    BOF3_INJECT(Area152_Record8Place);
    BOF3_INJECT(Area152_Record4Run);
    BOF3_INJECT(Area152_Record4MarkCell);
    BOF3_INJECT(Area152_DrawDrift);
    BOF3_INJECT(Area153_ChoiceFocusPair);
    BOF3_INJECT(Area154_ChoiceFocusPair);
    BOF3_INJECT(Area155_ChoiceGiveItem4D);
    BOF3_INJECT(Area155_ChoiceArmTail10);
    BOF3_INJECT(Area166_ChoiceFocusPair);
    BOF3_INJECT(Area167_ChoiceTail32At15);
    BOF3_INJECT(Area167_ChoiceTail32At14Even);
    BOF3_INJECT(Area167_ChoiceTail32At14);
    BOF3_INJECT(Area167_ChoiceTail32AtA);
    BOF3_INJECT(Area167_SetByteFE2);
    BOF3_INJECT(Area167_HeightBy4B4C);
    BOF3_INJECT(Area167_Toggle4BUnlessLow);
    BOF3_INJECT(Area167_Skip3Unless4B);
    BOF3_INJECT(Area167_HeightBy48To4A);
    BOF3_INJECT(Area167_Flags48By49);
    BOF3_INJECT(Area167_Skip3ByChapter);
    BOF3_INJECT(Area167_SetByteFE10);
    BOF3_INJECT(Area167_Tail32);
    BOF3_INJECT(Area167_ArriveHook);
    BOF3_INJECT(Area167_CellHook);
    BOF3_INJECT(Area167_Init);
}

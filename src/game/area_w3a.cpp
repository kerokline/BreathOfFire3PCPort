// World 3's areas 115..119: the code of the PSX's BIN/WORLD03/AREA115..119.EMI
// compiled into the exe at 0x418BE0..0x41A9CF - 56 functions (the band's 57
// less WorldMapHud_Start 0x419110, round eight's, which lies in area 115's
// block and is shared by the eleven world maps), each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). Round ten group AR3A; docs/area_w3a.md has the areas one
// section each.
//
// Area 115 is a world map (WorldMap_Records record 7): area 88's code
// (docs/area_w2b.md section 4, itself area 45's and area 16's) instruction
// for instruction over its own tables, with three operands of its own - the
// plate's animation bank 0x1C7, name sets of six bytes (five items, as area
// 65's) and a place hook that fills five text rows. So its 24 functions are
// one body of code here, over a WorldMapTables (area_w3a_callees.h), with a
// named entry per address; the body is area_w2b.cpp's, copied (the round's
// other world-map groups this wave copy it too: one shared body is a
// refactor for the rebinding pass, not a behaviour).
//
// Area 116: a choice that takes 10,000 Zenny, a handler that changes to area
// 100 (areas 36 and 59 name it too), a step hook that drops the party in
// (area 100's with two constants moved), an object trigger arming tail kind
// 4, effect kind 0xB8's state dispatcher and its ring state. Areas 117 and
// 118: a member frame the engine calls every frame in the area (EffectKind70_Run 0x46D780 by
// Game_AreaNumber), which turns the party members standing in a rectangle to
// the rectangle's facing (reversed while story flag 0x85 is set) and jumps
// them along it, its rectangle search, and a floor switch (a cell hook) that
// toggles flag 0x85 - one body over a TwinTables each. Areas 117..119: twelve
// handlers of one shape (Effect_Spawn at a party member, the kind from a
// table by the member's id), four choices, an object trigger, and a story-flag
// pair.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches either); reads by
// an unchecked index that stay in .data are kept. Every call goes through the
// harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for
// the callees; the callees of the group's own are called the same way, so each
// function is fuzzed alone.
#include "game/area_w3a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3a::at;
using U = std::uint32_t;
using area_harness::Handler;
using at::TwinTables;
using at::WorldMapTables;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Bank() { return At(at::kStoryFlags); }
unsigned char* PartyRecord(U index) { return At(at::kLeader + index * at::kPartyStride); }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }
void SetMessage(unsigned id) { SetWord(At(at::kMessage), id); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kFlagRow))))); }

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
// Areas 117 / 118's rectangle search, called by their member frames.
using RectFn = unsigned char (__cdecl*)(unsigned);
using RingFn = void (__cdecl*)(const long*);

}  // namespace

// ===========================================================================
// The world-map body (area 115's; area_w2b.cpp's copy for areas 87 and 88,
// itself area 45's code, docs/area_w1b.md section 5; area 16's in
// docs/area_w0b.md; area 33's in docs/worldmap_area.md sections 1..4)
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

// The field hook (WorldMap_FieldHooks; 0x56DE30 calls the entry of
// WorldMap_RecordIndex): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first place row whose word is the place 0x937F82, the row
//      count when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript. Any other:
//      the cell record whose (x, z) bytes are the leader's cell words (read
//      again after the call; searched with NO bound), its id byte +3; the
//      name set whose id it is (3 when none); up to set_stride - 1 Text_Records
//      rows from its item bytes (0xFF ends them; the rows end at
//      text_rows_end): "????????" and a 0 for an item whose byte at 0x9040EC
//      is 0, else the item's 16-byte name (0x669CD8 for item 0x16, else
//      Item_NamePtr(0, item + 0x38)); Msg_OpenScript(set + 0x16). Then
//      0x9039F4 (read again) + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 3 (which reads the plate state table's bytes) is checked.
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
        const unsigned cx = Word(At(at::kLeaderCellWordX));
        const unsigned cz = Word(At(at::kLeaderCellWordZ));
        U cell = t.cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = t.name_sets; a < t.name_sets_end; a += t.set_stride, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(t.name_sets + 1 + set * t.set_stride);
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
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(set + 0x16));
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
// areas 87 and 88, 0x803584 for area 45, 0x803588 for area 16).
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
// Area 115: the world map's eighth copy (WorldMap_Records record 7)
// ===========================================================================

// original 0x418BE0 (WorldMap_FieldHooks[7]): the place hook over area 115's
// tables - name sets of six bytes, five text rows (0x178 bytes, as area 88's).
extern "C" void __cdecl Area115_PlaceMessage(void) { PlaceMessage(at::kWm115); }
// original 0x418D60 (record 7 +0): the plate's run, Area115_PlateStates.
extern "C" void __cdecl Area115_PlateRun(void) { PlateRun(at::kWm115); }
// original 0x418E40 (plate state 0): bank 0x1C7.
extern "C" void __cdecl Area115_PlateStart(void) { PlateStart(at::kWm115); }
// original 0x418EA0 (plate state 1): Area115_PlateAnims searched with no bound.
extern "C" void __cdecl Area115_PlateShow(void) { PlateShow(at::kWm115); }
// original 0x418FF0 / 0x419040 / 0x4190A0 (plate states 2, 3, 4).
extern "C" void __cdecl Area115_PlateGrow(void) { PlateGrow(); }
extern "C" void __cdecl Area115_PlateHold(void) { PlateHold(); }
extern "C" void __cdecl Area115_PlateShrink(void) { PlateShrink(); }
// original 0x4190F0 (record 7 +0xC): Area115_HudStates by +1 (0: WorldMapHud_Start).
extern "C" void __cdecl Area115_HudRun(void) { StateEntry("Area115", "HudRun", at::kWm115.hud_states, 2, Cur()[1])(); }
// original 0x419130 (HUD state 1): the frame's step, then a tail jump to the box's.
extern "C" void __cdecl Area115_HudFrame(void) {
    area_harness::Phase(at::kWm115.fn_frame_step)();
    area_harness::Phase(at::kWm115.fn_box_step)();
}
// original 0x419140: Area115_FrameStates by +2 (0: WorldMap_FrameWait).
extern "C" void __cdecl Area115_FrameStep(void) { StateEntry("Area115", "FrameStep", at::kWm115.frame_states, 4, Cur()[2])(); }
// original 0x419160 / 0x419190 / 0x4191C0 (frame states 1, 2, 3).
extern "C" void __cdecl Area115_FrameSlideIn(void) { FrameSlideIn(at::kWm115); }
extern "C" void __cdecl Area115_FrameHold(void) { FrameHold(at::kWm115); }
extern "C" void __cdecl Area115_FrameSlideOut(void) { FrameSlideOut(at::kWm115); }
// original 0x419210: Area115_BoxStates by +3 (0: WorldMapHud_BoxWait).
extern "C" void __cdecl Area115_BoxStep(void) { StateEntry("Area115", "BoxStep", at::kWm115.box_states, 4, Cur()[3])(); }
// original 0x419230 / 0x4192A0 / 0x419310 (box states 1, 2, 3).
extern "C" void __cdecl Area115_BoxSlideIn(void) { BoxSlideIn(at::kWm115); }
extern "C" void __cdecl Area115_BoxHold(void) { BoxHold(at::kWm115); }
extern "C" void __cdecl Area115_BoxSlideOut(void) { BoxSlideOut(at::kWm115); }
// original 0x419370 / 0x419540 / 0x419600: the dial frame, one sprite, the region box.
extern "C" void __cdecl Area115_DrawFrame(int x, int y) { DrawFrame(at::kWm115, x, y); }
extern "C" void __cdecl Area115_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm115, x, y, index); }
extern "C" void __cdecl Area115_DrawHud(int x, int y) { DrawHud(at::kWm115, x, y); }
// original 0x419660 (record 7 +8): Area115_Record8States by +1 (0x4253C0, ours
// 0x419680, Area65_Record8Move 0x40C490).
extern "C" void __cdecl Area115_Record8Run(void) { StateEntry("Area115", "Record8Run", at::kWm115.record8_states, 3, Cur()[1])(); }
// original 0x419680 (record-8 state 1).
extern "C" void __cdecl Area115_Record8Place(void) { Record8Place(at::kWm115); }
// original 0x4197E0 (record 7 +4): Area115_Record4States by +1 (ours 0x419800,
// Area45_Record4Tick 0x408990).
extern "C" void __cdecl Area115_Record4Run(void) { StateEntry("Area115", "Record4Run", at::kWm115.record4_states, 2, Cur()[1])(); }
// original 0x419800 (record-4 state 0): Area115_Cells' record +0xB marked 0xA0.
extern "C" void __cdecl Area115_Record4MarkCell(void) { Record4MarkCell(at::kWm115); }
// original 0x4198C0 (record 7 +0x10): the drift layer, Area115_DriftUV.
extern "C" void __cdecl Area115_DrawDrift(void) { DrawDrift(at::kWm115); }

// ===========================================================================
// Area 116 (descriptor 0x620620): a choice, a handler, the step hook, object
// trigger 26, effect kind 0xB8's dispatcher and ring state. Its choice table's
// entries 1, 2 (0x420850, 0x420870), handler 1 (0x421FB0) are group AR3F's
// band, entries 3 and 4 Area61_ChoiceMark4 (group AR1D's); its effect state 0
// Area59_EffectGround (AR1D's).
// ===========================================================================

// original 0x419D30 (area 116's choice 0, 0x620608): the cursor not 0: message
// 4 and the byte 0x9398CF 6. Else Party_Zenny 10,000 or more (unsigned):
// message 2 and 10,000 taken; less: message 3 and 0x9398CF 6.
extern "C" void __cdecl Area116_ChoicePay10000(void) {
    if (At(at::kCursor)[0] != 0) {
        SetMessage(4);
        At(at::kByte9398CF)[0] = 6;
        return;
    }
    const U zenny = static_cast<U>(Long(At(at::kZenny)));
    if (zenny >= 10000) {
        SetMessage(2);
        SetLong(At(at::kZenny), static_cast<std::int32_t>(zenny - 10000));
        return;
    }
    SetMessage(3);
    At(at::kByte9398CF)[0] = 6;
}

// original 0x419D80 (handler 0 of areas 116, 36 and 59; PSX 0x801F2D54):
// Field_ChangeArea(100, 0x460000, 0x340000, 0x81).
extern "C" void __cdecl Area116_ToArea100(void) { AH_CALL(Field_ChangeArea)(0x64, 0x460000, 0x340000, 0x81); }

// original 0x419DA0 (Area_StepHook's case for area 116; Area100_StepHook
// 0x414330 with two constants moved): Cond_ByteFD 2, story flag 0x33, x's
// high word 0x46..0x48 and z's 0x33..0x35 (16-bit compares), the leader's pose
// 0, 7 or 6: counter 0 = 0, Party_DropIn(0), al 1. Else al 0.
extern "C" unsigned char __cdecl Area116_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    if (AH_CALL(Flags_Test)(Bank(), 0x33) == 0) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x46) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(z) - 0x33) >= 3) return 0;
    const unsigned char pose = At(at::kLeaderDir)[0];
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    At(at::kCounter0)[0] = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x419E00 (Field_ObjectTriggers id 26; a gap of the tool, area
// 116's block): ScriptFlags_Set40; tail kind 4 with its argument 8 (the state
// byte 0x9039F4 not written). al 0.
extern "C" unsigned char __cdecl Area116_Trigger26(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    At(at::kTailKind)[0] = 4;
    At(at::kTailArg)[0] = 8;
    return 0;
}

// original 0x419E20 (Effect_KindHandlers[0xB8], 0x655630; a gap of the
// tool): Area116_EffectStates by Sprite_Current[1] (the running effect
// record).
extern "C" void __cdecl Area116_EffectB8Run(void) {
    StateEntry("Area116", "EffectB8Run", at::kA116EffectStates, at::kA116EffectStateCount, Cur()[1])();
}

// original 0x419E40 (Area116_EffectStates[1]; Area100_EffectB7Ring 0x4144B0
// instruction for instruction): the point (x +0x34, z +0x38, y +0x3C) of the
// running record copied to the stack and handed to 0x4220D0. The fourth stack
// dword is not written by the original (a stale word the callee does not
// read): ours passes 0.
extern "C" void __cdecl Area116_EffectB8Ring(void) {
    const unsigned char* const cur = Sprite_Current;
    long point[4];
    point[0] = Long(cur + 0x34);
    point[1] = Long(cur + 0x38);
    point[2] = Long(cur + 0x3C);
    point[3] = 0;
    AH_AT(RingFn, area_w3a::kRingAt)(point);
}

// ===========================================================================
// Areas 117..119's member spawns: one shape, twelve handlers
// ===========================================================================

namespace {

// Party record `member`'s words +0x30 (z) and +0x2E (x), the member's id from
// the first party list; Sprite_Current made that record; Effect_Spawn(kind, 0,
// the table's byte by the id (unchecked, as the original's), x, z); an answer
// other than 0xFF stored to Sprite_Current[0xB], Sprite_Current read again
// after the call (area 11's Area11_SpawnEffect is the same shape).
void SpawnAtMember(U member, U table, unsigned char kind) {
    const unsigned char* const rec = PartyRecord(member);
    const auto z = static_cast<short>(Word(rec + 0x30));
    const auto x = static_cast<short>(Word(rec + 0x2E));
    const unsigned id = At(at::kPartyList + member)[0];
    Sprite_Current = PartyRecord(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(At(table + id)[0]), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

}  // namespace

// original 0x419E70 / 0x419EC0 / 0x419F10 / 0x419F60 (area 117's handlers 0..3,
// Area117_Handlers 0x621588; PSX 0x801F3D70 / 0x801F3DF0 / 0x801F3E70 /
// 0x801F3EF0): party member 2, kinds 3, 4, 5, 2.
extern "C" void __cdecl Area117_Spawn3AtMember2(void) { SpawnAtMember(2, at::kSpawnTable117A, 3); }
extern "C" void __cdecl Area117_Spawn4AtMember2(void) { SpawnAtMember(2, at::kSpawnTable117B, 4); }
extern "C" void __cdecl Area117_Spawn5AtMember2(void) { SpawnAtMember(2, at::kSpawnTable117B, 5); }
extern "C" void __cdecl Area117_Spawn2AtMember2(void) { SpawnAtMember(2, at::kSpawnTable117C, 2); }
// original 0x41A3C0 (area 118's handler 0, 0x621DE0; PSX 0x801F4B88): the leader, kind 1.
extern "C" void __cdecl Area118_Spawn1AtLeader(void) { SpawnAtMember(0, at::kSpawnTable118, 1); }
// original 0x41A730 .. 0x41A910 (area 119's handlers 0, 4..9 = choices 1, 5..10,
// Area119_Handlers 0x622AE8; PSX 0x801F4138, 0x801F4224, 0x801F42A4,
// 0x801F4324, 0x801F43A4, 0x801F4424, 0x801F44A4).
extern "C" void __cdecl Area119_Spawn1AtLeader(void) { SpawnAtMember(0, at::kSpawnTable119A, 1); }
extern "C" void __cdecl Area119_Spawn4AtLeader(void) { SpawnAtMember(0, at::kSpawnTable119A, 4); }
extern "C" void __cdecl Area119_Spawn1AtLeaderB(void) { SpawnAtMember(0, at::kSpawnTable119B, 1); }
extern "C" void __cdecl Area119_Spawn1AtMember1(void) { SpawnAtMember(1, at::kSpawnTable119B, 1); }
extern "C" void __cdecl Area119_Spawn1AtMember2(void) { SpawnAtMember(2, at::kSpawnTable119B, 1); }
extern "C" void __cdecl Area119_Spawn3AtMember2(void) { SpawnAtMember(2, at::kSpawnTable119B, 3); }
extern "C" void __cdecl Area119_Spawn4AtMember2(void) { SpawnAtMember(2, at::kSpawnTable119A, 4); }

// ===========================================================================
// Area 117 (descriptor 0x6215B0): its trigger and choices; areas 117 and 118's
// member frames, rectangle searches and floor switches
// ===========================================================================

// original 0x419FB0 (Field_ObjectTriggers id 33; a gap of the tool; PSX
// 0x801F3F70 by the gap): ScriptFlags_Set40; tail kind 0x2C (engine,
// 0x56DE50) armed, state 0, sub-kind 5. al 0.
extern "C" unsigned char __cdecl Area117_Trigger33(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    At(at::kTailKind)[0] = 0x2C;
    At(at::kTailState)[0] = 0;
    At(at::kTailArg)[0] = 5;
    return 0;
}

// original 0x419FD0 (area 117's choice 0 and handler 4; PSX 0x801F3FB0): no new
// message; the cursor 0 arms tail kind 0xA (state 0, argument 0x1C).
extern "C" void __cdecl Area117_ChoiceArmTailA(void) {
    const unsigned char cursor = At(at::kCursor)[0];
    SetMessage(0xFFFF);
    if (cursor != 0) return;
    At(at::kTailKind)[0] = 0xA;
    At(at::kTailState)[0] = 0;
    At(at::kTailArg)[0] = 0x1C;
}

// original 0x41A000 (choice 1 and handler 5; PSX 0x801F3FF0): the message
// Area117_Messages1[the cursor, s8 - unchecked, a negative one reads before
// the table]; a cursor 0..4: counter 0 = 0xA and Sound_PlayEffect(0x205).
extern "C" void __cdecl Area117_ChoiceMessage1(void) {
    const auto cursor = static_cast<signed char>(At(at::kCursor)[0]);
    SetMessage(Word(At(at::kA117Messages1 + static_cast<U>(cursor * 2))));
    if (cursor < 0 || cursor > 4) return;
    At(at::kCounter0)[0] = 0xA;
    AH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x41A040 (choice 2 and handler 6; PSX 0x801F4048): the message
// Area117_Messages2[the cursor, s8, unchecked]; then by a five-entry jump
// table in the function (0x41A080) for a cursor 0..4 (unsigned): 2 sets
// counter 0 = 0x14; 0, 1, 3, 4 counter 0 = 0xA and Sound_PlayEffect(0x205).
extern "C" void __cdecl Area117_ChoiceMessage2(void) {
    const int cursor = static_cast<signed char>(At(at::kCursor)[0]);
    SetMessage(Word(At(at::kA117Messages2 + static_cast<U>(cursor * 2))));
    if (static_cast<U>(cursor) > 4) return;
    if (cursor == 2) {
        At(at::kCounter0)[0] = 0x14;
        return;
    }
    At(at::kCounter0)[0] = 0xA;
    AH_CALL(Sound_PlayEffect)(0x205);
}

namespace {

// The rectangle search (areas 117 / 118's MemberRect): party record `member`
// (& 0xFF; unchecked against the three records)'s x +0x34 and z +0x38 against
// each (x0, z0, x1, z1) of the table, cell bytes << 16, signed and inclusive:
// the first containing it, or 0xFF.
unsigned char MemberRect(const TwinTables& t, unsigned member) {
    const unsigned char* const rec = PartyRecord(member & 0xFF);
    const std::int32_t x = Long(rec + 0x34);
    const std::int32_t z = Long(rec + 0x38);
    unsigned index = 0;
    for (U r = t.rects; r < t.rects_end; r += 6, ++index) {
        const unsigned char* const e = At(r);
        if (x < static_cast<std::int32_t>(static_cast<U>(e[0]) << 16)) continue;
        if (x > static_cast<std::int32_t>(static_cast<U>(e[2]) << 16)) continue;
        if (z < static_cast<std::int32_t>(static_cast<U>(e[1]) << 16)) continue;
        if (z <= static_cast<std::int32_t>(static_cast<U>(e[3]) << 16)) return static_cast<unsigned char>(index);
    }
    return 0xFF;
}

// The member frame (areas 117 / 118's, called by EffectKind70_Run 0x46D780 every field frame
// in the area). Field_ScriptFlags bit 13 cleared; for each member m below
// Field_MemberCount (read again after each member; m a byte), with the bit b
// = 1 << (m & 31) and its low byte b8 (0 for m & 31 of 8 and more, as the
// original's 8-bit shift): Field_State and Sprite_Current made record m, then
// the rectangle search (through the copy's own address). Outside every
// rectangle: a member marked in the caller's +0xB (b8) is unmarked there and
// in Field_ScriptFlags2 (~b, 16 bits). Inside, not yet marked, and b in
// neither Field_ScriptFlags2 nor Field_ScriptFlags: Field_State +0x128 = 2,
// +9 = 0, marked in +0xB and Field_ScriptFlags2; +8 = the rectangle's facing,
// ^ 4 while its story flag is set; Sprite_EnsureAnimation(+8). Inside and
// marked: +9 0 - Field_JumpSetUp, then Field_JumpCamera for member 0 only;
// else the facing (^ 4 by the flag) and, when +8 is not it, +8 = it, the
// three dwords +0xC, +0x10, +0x14 negated and +9 = 8 - +9; then
// Field_LeaderStepTick, +9 - 1, Sprite_ScriptTick and Field_ScriptFlags bit
// 13 set. After the members: Field_Request 5 clears bit 13 again;
// Sprite_Current put back (Field_State is not). Every Sprite_Current use is a
// fresh read, as the original's.
void MembersFrame(const TwinTables& t) {
    unsigned count = Field_MemberCount;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    unsigned char* const caller = Sprite_Current;
    if (count != 0) {
        unsigned m = 0;
        do {
            unsigned char* const rec = PartyRecord(m);
            Field_State = rec;
            Sprite_Current = rec;
            const unsigned char found = AH_AT(RectFn, t.fn_member_rect)(m);
            const U bit = 1u << (m & 31);
            const auto bit8 = static_cast<unsigned char>((m & 31) < 8 ? bit : 0);
            if (found == 0xFF) {
                const unsigned char marks = caller[0xB];
                if ((marks & bit8) != 0) {
                    caller[0xB] = static_cast<unsigned char>(marks & ~bit8);
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit);
                }
            } else if ((caller[0xB] & bit8) == 0) {
                if (((static_cast<U>(Field_ScriptFlags2) | static_cast<U>(Field_ScriptFlags)) & bit) == 0) {
                    Field_State[at::kFieldStateMember] = 2;
                    Sprite_Current[9] = 0;
                    caller[0xB] = static_cast<unsigned char>(caller[0xB] | bit8);
                    unsigned char* const cur = Sprite_Current;
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | bit);
                    const U e = t.rects + 4 + found * 6u;
                    cur[8] = At(e)[0];
                    if (AH_CALL(Flags_Test)(Bank(), At(e + 1)[0]) != 0) Sprite_Current[8] ^= 4;
                    AH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
                }
            } else {
                if (Sprite_Current[9] == 0) {
                    AH_CALL(Field_JumpSetUp)();
                    if (m == 0) AH_CALL(Field_JumpCamera)();
                } else {
                    const U e = t.rects + 4 + found * 6u;
                    unsigned char facing = At(e)[0];
                    if (AH_CALL(Flags_Test)(Bank(), At(e + 1)[0]) != 0) facing ^= 4;
                    unsigned char* o = Sprite_Current;
                    if (o[8] != facing) {
                        o[8] = facing;
                        o = Sprite_Current;
                        SetLong(o + 0xC, -Long(o + 0xC));
                        o = Sprite_Current;
                        SetLong(o + 0x10, -Long(o + 0x10));
                        o = Sprite_Current;
                        SetLong(o + 0x14, -Long(o + 0x14));
                        o = Sprite_Current;
                        o[9] = static_cast<unsigned char>(8 - o[9]);
                    }
                }
                AH_CALL(Field_LeaderStepTick)();
                Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
                AH_CALL(Sprite_ScriptTick)();
                Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x2000);
            }
            count = Field_MemberCount;
            m = (m + 1) & 0xFF;
        } while (m < count);
    }
    if (Field_Request == 5) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    Sprite_Current = caller;
}

// The floor switch (areas 117 / 118's cell hook, Area_CellHooks): the first
// (x, z, facing, flag) record whose x and z are the argument bytes and facing
// the leader's +8 (read before the search); none (the index at the table's
// count) al 0. Story flag 0x1C set al 0. Else Flags_Toggle of the record's
// flag, Effect_HoldFlag1C(0xF), Sound_PlayEffect(0x203), al 1.
unsigned char SwitchHook(const TwinTables& t, unsigned x, unsigned z) {
    const auto cx = static_cast<unsigned char>(x);
    const auto cz = static_cast<unsigned char>(z);
    const unsigned char facing = At(at::kLeaderDir)[0];
    unsigned index = 0;
    for (U r = t.switches; r < t.switches_end; r += 4, ++index) {
        const unsigned char* const e = At(r);
        if (e[0] == cx && e[1] == cz && e[2] == facing) break;
    }
    if (index == t.switch_count) return 0;
    if (AH_CALL(Flags_Test)(Bank(), 0x1C) != 0) return 0;
    AH_CALL(Flags_Toggle)(Bank(), At(t.switches + index * 4u + 3)[0]);
    AH_CALL(Effect_HoldFlag1C)(0xF);
    AH_CALL(Sound_PlayEffect)(0x203);
    return 1;
}

}  // namespace

// original 0x41A0A0 (called by 0x46D7A9, EffectKind70_Run's case for area 117): the
// member frame over Area117_Rects.
extern "C" void __cdecl Area117_MembersFrame(void) { MembersFrame(at::kTw117); }
// original 0x41A2D0 (called by Area117_MembersFrame; a gap of the tool): one
// rectangle, Area117_Rects 0x6215A8.
extern "C" unsigned char __cdecl Area117_MemberRect(unsigned member) { return MemberRect(at::kTw117, member); }
// original 0x41A340 (Area_CellHooks' entry for area 0x75): one switch,
// Area117_Switches 0x62160C.
extern "C" unsigned char __cdecl Area117_SwitchHook(unsigned x, unsigned z) { return SwitchHook(at::kTw117, x, z); }
// original 0x41A410 (called by 0x46D7B6, area 118's case): area 117's
// 0x41A0A0 instruction for instruction over area 118's tables.
extern "C" void __cdecl Area118_MembersFrame(void) { MembersFrame(at::kTw118); }
// original 0x41A640: Area118_Rects 0x621DF0.
extern "C" unsigned char __cdecl Area118_MemberRect(unsigned member) { return MemberRect(at::kTw118, member); }
// original 0x41A6B0 (Area_CellHooks' entry for area 0x76): Area118_Switches 0x621E3C.
extern "C" unsigned char __cdecl Area118_SwitchHook(unsigned x, unsigned z) { return SwitchHook(at::kTw118, x, z); }

// ===========================================================================
// Area 119 (descriptor 0x622B18): the story-flag pair and choice 0. Its
// handlers 1..3 (choices 2..4) are 0x41F320 (group AR3E's band), 0x42A4B0
// (a later band) and Area80_ResetCameraShift.
// ===========================================================================

// original 0x41A960 (handler 10 / choice 11; PSX 0x801F4524): flag 0x3C of the
// chapter's row set, Sound_PlayEffect(0x203).
extern "C" void __cdecl Area119_SetRowFlag3C(void) {
    AH_CALL(Flags_Set)(FlagRow(), 0x3C);
    AH_CALL(Sound_PlayEffect)(0x203);
}
// original 0x41A980 (handler 11 / choice 12; PSX 0x801F4554): the same flag cleared.
extern "C" void __cdecl Area119_ClearRowFlag3C(void) {
    AH_CALL(Flags_Clear)(FlagRow(), 0x3C);
    AH_CALL(Sound_PlayEffect)(0x203);
}
// original 0x41A9A0 (choice 0; PSX 0x801F4584 by the table's anchor): no new
// message; the cursor (s8) 0: counter 0 = 0xA, 1: 0x14, else nothing.
extern "C" void __cdecl Area119_ChoiceCounter0(void) {
    const auto cursor = static_cast<signed char>(At(at::kCursor)[0]);
    SetMessage(0xFFFF);
    if (cursor == 0) At(at::kCounter0)[0] = 0xA;
    else if (cursor == 1) At(at::kCounter0)[0] = 0x14;
}

void AreaW3a_Inject() {
    if (bof3::WantsShadow("area_w3a")) area_w3a::SelfTest();
    BOF3_INJECT(Area115_PlaceMessage);
    BOF3_INJECT(Area115_PlateRun);
    BOF3_INJECT(Area115_PlateStart);
    BOF3_INJECT(Area115_PlateShow);
    BOF3_INJECT(Area115_PlateGrow);
    BOF3_INJECT(Area115_PlateHold);
    BOF3_INJECT(Area115_PlateShrink);
    BOF3_INJECT(Area115_HudRun);
    BOF3_INJECT(Area115_HudFrame);
    BOF3_INJECT(Area115_FrameStep);
    BOF3_INJECT(Area115_FrameSlideIn);
    BOF3_INJECT(Area115_FrameHold);
    BOF3_INJECT(Area115_FrameSlideOut);
    BOF3_INJECT(Area115_BoxStep);
    BOF3_INJECT(Area115_BoxSlideIn);
    BOF3_INJECT(Area115_BoxHold);
    BOF3_INJECT(Area115_BoxSlideOut);
    BOF3_INJECT(Area115_DrawFrame);
    BOF3_INJECT(Area115_DrawSprite);
    BOF3_INJECT(Area115_DrawHud);
    BOF3_INJECT(Area115_Record8Run);
    BOF3_INJECT(Area115_Record8Place);
    BOF3_INJECT(Area115_Record4Run);
    BOF3_INJECT(Area115_Record4MarkCell);
    BOF3_INJECT(Area115_DrawDrift);
    BOF3_INJECT(Area116_ChoicePay10000);
    BOF3_INJECT(Area116_ToArea100);
    BOF3_INJECT(Area116_StepHook);
    BOF3_INJECT(Area116_Trigger26);
    BOF3_INJECT(Area116_EffectB8Run);
    BOF3_INJECT(Area116_EffectB8Ring);
    BOF3_INJECT(Area117_Spawn3AtMember2);
    BOF3_INJECT(Area117_Spawn4AtMember2);
    BOF3_INJECT(Area117_Spawn5AtMember2);
    BOF3_INJECT(Area117_Spawn2AtMember2);
    BOF3_INJECT(Area117_Trigger33);
    BOF3_INJECT(Area117_ChoiceArmTailA);
    BOF3_INJECT(Area117_ChoiceMessage1);
    BOF3_INJECT(Area117_ChoiceMessage2);
    BOF3_INJECT(Area117_MembersFrame);
    BOF3_INJECT(Area117_MemberRect);
    BOF3_INJECT(Area117_SwitchHook);
    BOF3_INJECT(Area118_Spawn1AtLeader);
    BOF3_INJECT(Area118_MembersFrame);
    BOF3_INJECT(Area118_MemberRect);
    BOF3_INJECT(Area118_SwitchHook);
    BOF3_INJECT(Area119_Spawn1AtLeader);
    BOF3_INJECT(Area119_Spawn4AtLeader);
    BOF3_INJECT(Area119_Spawn1AtLeaderB);
    BOF3_INJECT(Area119_Spawn1AtMember1);
    BOF3_INJECT(Area119_Spawn1AtMember2);
    BOF3_INJECT(Area119_Spawn3AtMember2);
    BOF3_INJECT(Area119_Spawn4AtMember2);
    BOF3_INJECT(Area119_SetRowFlag3C);
    BOF3_INJECT(Area119_ClearRowFlag3C);
    BOF3_INJECT(Area119_ChoiceCounter0);
}

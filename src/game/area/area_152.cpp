// Moved here by tools/move_funcs.py from area_w4a.cpp (docs/source-layout-round.md).
#include "game/area/area_152.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using area_harness::Handler;

using at::WorldMapTables;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

void Clear(unsigned index) { AH_CALL(Flags_Clear)(Bank(), index); }

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

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
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
    const auto state = AreaW0c_TailState;
    if (state == 1) {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        AreaW0c_TailSub = 0;
        return;
    }
    if (state != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    const unsigned place = AreaW0b_Place;
    std::int32_t row = 0;
    for (U a = t.place_rows; a < t.place_rows_end; a += 0x20, ++row)
        if (Word(At(a)) == place) break;
    const std::int32_t index = (row << 4) + Cond_ByteFA;
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(t.place_rows + static_cast<U>(index * 2)))));
    const auto next = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
    Field_Request = 2;
    AreaW0c_TailState = next;
}

// The place plate (the record's +0: effect kind 0). The cell ahead of the
// leader - the high words of (leader +9) * (+0xC) + (+0x34) and of the same
// with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the kind +0xB by
// AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2, 0xAE 3, else
// Field_ScriptFlags2 bit 12 4, else 0; then the plate state table by +1
// (read after the calls; ours aborts past its five entries).
void PlateRun(const WorldMapTables& t) {
    const U steps = ObjTrio[0].step_frames;
    const U zs = steps * static_cast<std::uint32_t>(ObjTrio[0].step_z) + static_cast<std::uint32_t>(ObjTrio[0].z);
    const U xs = steps * static_cast<std::uint32_t>(ObjTrio[0].step_x) + static_cast<std::uint32_t>(ObjTrio[0].x);
    const auto x = static_cast<short>(xs >> 16);
    const auto z = static_cast<short>(zs >> 16);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, z);
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
        const unsigned place = AreaW0b_Place;
        U entry = t.plate_anims;
        while (Word(At(entry)) != place) entry += 4;
        rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->work_18, static_cast<std::int32_t>(place));
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
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_x, 0);
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_y, 0x10000);
    Cur()[0x48] = 2;
    Cur()[9] = 8;
    AH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Cur()[1] = 2;
}

// The plate's state 2: WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0
// +0x48 = 0 and +1 = 3; then a tail jump to Sprite_QueueOverlay.
void PlateGrow() {
    AH_CALL(WorldMap_PinSprite)();
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_x, static_cast<std::int32_t>(static_cast<U>(rv::Current<records::EffectObject>()->scale_x) + 0x2000));
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
                            (shown == 1 && static_cast<U>(rv::As<records::EffectObject>(o)->work_18) != AreaW0b_Place);
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
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_x, static_cast<std::int32_t>(static_cast<U>(rv::Current<records::EffectObject>()->scale_x) - 0x2000));
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
    rv::Current<records::EffectWork>()->work_2e = static_cast<std::uint16_t>(rv::Current<records::EffectWork>()->work_2e) + 0x10u;
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(t.fn_frame_hold)();
}

// State 2: the mode byte 2 makes +2 = 3; the copy's DrawFrame(0x10, the word
// +0x2E). The y pushed carries a stale high half the drawing never reads:
// ours passes the word sign-extended.
void FrameHold(const WorldMapTables& t) {
    if (AreaW0b_MapMode == 2) Cur()[2] = 3;
    AH_WORD(DrawAt, t.fn_draw_frame)(0x10, S16(Cur() + 0x2E));
}

// State 3: the word +0x2E -= 0x10; at -0x30 and below +2 = 0; unless the
// mode byte is 2, +2 = 1 (over the 0); DrawFrame(0x10, y).
void FrameSlideOut(const WorldMapTables& t) {
    rv::Current<records::EffectWork>()->work_2e = static_cast<std::uint16_t>(rv::Current<records::EffectWork>()->work_2e) - 0x10u;
    unsigned char* o = Cur();
    if (S16(o + 0x2E) <= -0x30) {
        o[2] = 0;
        o = Cur();
    }
    if (AreaW0b_MapMode != 2) {
        o[2] = 1;
        o = Cur();
    }
    AH_WORD(DrawAt, t.fn_draw_frame)(0x10, S16(o + 0x2E));
}

// The region box, state 1: y +0x30 -= 10; at 0xC8 and below (s16) +3 + 1;
// when the box leaves +3 = 3; the copy's DrawHud(0x5C, y) - the y pushed
// with the object pointer's high half above the word, which the drawing
// never reads.
void BoxSlideIn(const WorldMapTables& t) {
    rv::Current<records::EffectObject>()->screen_y = static_cast<std::uint16_t>(rv::Current<records::EffectObject>()->screen_y) - 10u;
    unsigned char* o = Cur();
    if (S16(o + 0x30) <= 0xC8) {
        o[3] = static_cast<unsigned char>(o[3] + 1);
        o = Cur();
    }
    if (BoxLeaves(o)) {
        o[3] = 3;
        o = Cur();
    }
    AH_WORD(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
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
    AH_WORD(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// State 3: y +0x30 += 10; at 0xF0 and above (s16) +3 = 0; unless the mode
// byte is set (whatever +0xB), Field_Request is 2 or Field_ScriptFlags bit
// 8, +3 = 1; DrawHud(0x5C, y).
void BoxSlideOut(const WorldMapTables& t) {
    rv::Current<records::EffectObject>()->screen_y = static_cast<std::uint16_t>(rv::Current<records::EffectObject>()->screen_y) + 10u;
    unsigned char* o = Cur();
    if (S16(o + 0x30) >= 0xF0) {
        o[3] = 0;
        o = Cur();
    }
    if (AreaW0b_MapMode == 0 && Field_Request != 2 && (Field_ScriptFlags & 0x100) == 0) {
        o[3] = 1;
        o = Cur();
    }
    AH_WORD(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
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
    world_map_legend::Begin();   // DIV-0135: which page; the port's key names painted over
    const auto sprite = AH_WORD(DrawSpriteAt, t.fn_draw_sprite);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    sprite(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellX.v)))),
                                                       static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellZ.v)))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) sprite(x + 0x30, y, 1);
    world_map_legend::Key(sprite, t.buttons, 6, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000))), x + 0x38, y + 8,
                          withheld1);
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) sprite(x + 0x30, y + 0x10, 2);
    world_map_legend::Key(sprite, t.buttons, 8, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c))), x + 0x38, y + 0x10,
                          withheld2);
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellX.v)))),
                                            static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellZ.v))))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (AreaW0b_PartySet & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) sprite(x + 0x30, y + 0x18, 3);
    world_map_legend::Start(x + 0x30, y + 0x18, !third);
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
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->clut, 0x7B80);
    const unsigned char* const entry = At(t.sprites + (index & 0xFF) * 4u);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->w, entry[0]);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->h, entry[1]);
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
    AH_WORD(DrawSpriteAt, t.fn_draw_sprite)(x, y, 4);
    AH_WORD(DrawSpriteAt, t.fn_draw_sprite)(x + 0x80, y, 5);
    const unsigned char* const text = At(::bof3::Img(at::kAreaText.v) + (static_cast<U>(Long(At(t.label_offset))) & 0xFFFF));
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
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, dir(Cur(), 0));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, dir(Cur(), 2));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, ObjTrio[0].x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, ObjTrio[0].z);
    unsigned char* o = Cur();
    auto* tO = rv::As<records::EffectObject>(o);
    if (o[6] != 0) {
        rv::Store<unsigned>(rv::As<records::EffectObject>(o)->x_whole, rv::As<records::EffectObject>(o)->x_whole - static_cast<unsigned>(dir(o, 0) >> 13));
        o = Cur();
        tO = rv::As<records::EffectObject>(o);
        rv::Store<unsigned>(rv::As<records::EffectObject>(o)->z_whole, rv::As<records::EffectObject>(o)->z_whole - static_cast<unsigned>(dir(o, 2) >> 13));
        o = Cur();
        tO = rv::As<records::EffectObject>(o);
        const bool along_x = tO->step_x == 0;
        const unsigned step = o[6] == 1 ? 2u : 0xFFFEu;
        if (along_x) rv::Store<unsigned>(rv::As<records::EffectObject>(o)->x_whole, rv::As<records::EffectObject>(o)->x_whole + step);
        else rv::Store<unsigned>(rv::As<records::EffectObject>(o)->z_whole, rv::As<records::EffectObject>(o)->z_whole + step);
        o = Cur();
        tO = rv::As<records::EffectObject>(o);
    }
    rv::Store<unsigned>(rv::As<records::EffectObject>(o)->x_whole, rv::As<records::EffectObject>(o)->x_whole - static_cast<unsigned>(dir(o, 0) >> 9));
    o = Cur();
    tO = rv::As<records::EffectObject>(o);
    rv::Store<unsigned>(rv::As<records::EffectObject>(o)->z_whole, rv::As<records::EffectObject>(o)->z_whole - static_cast<unsigned>(dir(o, 2) >> 9));
    Cur()[0xB] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    AH_CALL(Sprite_SetAnimation)(At(t.record8_anims + Cur()[8] * 2u)[0]);
    o = Cur();
    tO = rv::As<records::EffectObject>(o);
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
    if (CharacterRecords[0].char_id == 9 || (Field_StatusBits & 1) != 0) {
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
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[z * width + x] = 0xA0;
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
        rv::Store<unsigned>(rv::As<records::SpriteHead>(o)->z_whole, rv::As<records::SpriteHead>(o)->z_whole + (Frame_Counter & 0xF));
        Cur()[2] = static_cast<unsigned char>(Cur()[2] + 1);
        o = Cur();
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const std::int32_t b = o[0xB];
    const std::int32_t e = b - 2;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->z, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(o)->z) + (static_cast<U>(b) << 10)));
    o = Cur();
    if (S16(o + 0x3A) > AreaMap_Header[1] + 8) {
        rv::Store<unsigned>(rv::As<records::SpriteHead>(o)->z_whole, 0xFFF8);
        o = Cur();
    }
    std::int32_t dx = S16(At(::bof3::Img(at::kLeaderCellX.v))) - S16(o + 0x36);
    if (dx < 0) dx = -dx;
    if (dx > 0x19) {
        std::int32_t dz = S16(At(::bof3::Img(at::kLeaderCellZ.v))) - S16(o + 0x3A);
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
    const std::int32_t cx = (rv::As<records::SpriteHead>(o)->x >> 9) - 0x3FC0;
    const std::int32_t cz = (rv::As<records::SpriteHead>(o)->z >> 9) - 0x3FC0;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    rv::Store<unsigned>(rv::As<records::SVector>(v)[0].z, 0xFD00);
    rv::Store<unsigned>(rv::As<records::SVector>(v)[2].y, static_cast<unsigned>(cz + r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[3].y, static_cast<unsigned>(cz + r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[1].x, static_cast<unsigned>(cx + r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[3].x, static_cast<unsigned>(cx + r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[0].x, static_cast<unsigned>(cx - r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[2].x, static_cast<unsigned>(cx - r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[0].y, static_cast<unsigned>(cz - r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[1].y, static_cast<unsigned>(cz - r));
    rv::Store<unsigned>(rv::As<records::SVector>(v)[1].z, 0xFD00);
    rv::Store<unsigned>(rv::As<records::SVector>(v)[2].z, 0xFD00);
    rv::Store<unsigned>(rv::As<records::SVector>(v)[3].z, 0xFD00);
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
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(q)->clut, 0x78CB);
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(q)->tpage, 0x5B);
            q[4] = q[5] = q[6] = 0x28;
            const U m = At(t.drift_size + static_cast<U>(e))[0];
            const U ubase = At(t.drift_u + static_cast<U>(e))[0];
            const U vbase = At(t.drift_v + static_cast<U>(e))[0];
            const U scroll = (rv::Current<records::SpriteHead>()->z_frac * m) >> 16;
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
        unsigned char* const e = At(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
        const long z = ObjTrio[0].z;
        const long x = ObjTrio[0].x;
        e[0] = 1;
        e[5] = 0x16;
        e[1] = 1;
        e[6] = static_cast<unsigned char>(k);
        e[8] = static_cast<unsigned char>(r >> 4);
        rv::Store<unsigned>(rv::As<records::EffectObject>(e)->height_whole, static_cast<unsigned>(AH_CALL(AreaMap_Elevation)(x, z)) + 0x400u);
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

void Area152_Inject() {
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
}

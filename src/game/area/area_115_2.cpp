// Moved here by tools/move_funcs.py from area_w3a.cpp (docs/source-layout-round.md).
#include "game/area/area_115_2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using area_harness::Handler;

using at::WorldMapTables;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

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

using RingFn = void (__cdecl*)(const long*);

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
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
// areas 87 and 88, 0x803584 for area 45, 0x803588 for area 16).
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

// original 0x419D30 (area 116's choice 0, 0x620608): the cursor not 0: message
// 4 and the byte 0x9398CF 6. Else Party_Zenny 10,000 or more (unsigned):
// message 2 and 10,000 taken; less: message 3 and 0x9398CF 6.
extern "C" void __cdecl Area116_ChoicePay10000(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0) {
        SetMessage(4);
        AreaW0c_AnswerMark = 6;
        return;
    }
    const U zenny = Party_Zenny;
    if (zenny >= 10000) {
        SetMessage(2);
        Party_Zenny = static_cast<std::int32_t>(zenny - 10000);
        return;
    }
    SetMessage(3);
    AreaW0c_AnswerMark = 6;
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
    const unsigned char pose = ObjTrio[0].direction;
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    At(::bof3::Img(at::kCounter0.v))[0] = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x419E00 (Field_ObjectTriggers id 26; a gap of the tool, area
// 116's block): ScriptFlags_Set40; tail kind 4 with its argument 8 (the state
// byte 0x9039F4 not written). al 0.
extern "C" unsigned char __cdecl Area116_Trigger26(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = 4;
    AreaW0c_TailSub = 8;
    return 0;
}

// original 0x419E20 (Effect_KindHandlers[0xB8], 0x655630; a gap of the
// tool): Area116_EffectStates by Sprite_Current[1] (the running effect
// record).
extern "C" void __cdecl Area116_EffectB8Run(void) {
    StateEntry("Area116", "EffectB8Run", ::bof3::Img(Area116_EffectStates_addr), at::kA116EffectStateCount, Cur()[1])();
}

// original 0x419E40 (Area116_EffectStates[1]; Area100_EffectB7Ring 0x4144B0
// instruction for instruction): the point (x +0x34, z +0x38, y +0x3C) of the
// running record copied to the stack and handed to 0x4220D0. The fourth stack
// dword is not written by the original (a stale word the callee does not
// read): ours passes 0.
extern "C" void __cdecl Area116_EffectB8Ring(void) {
    const unsigned char* const cur = Sprite_Current;
    long point[4];
    point[0] = rv::As<records::SpriteHead>(cur)->x;
    point[1] = rv::As<records::SpriteHead>(cur)->z;
    point[2] = rv::As<records::SpriteHead>(cur)->height;
    point[3] = 0;
    AH_AT(RingFn, area_w3a::kRingAt.v)(point);
}

void Area1152_Inject() {
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
}

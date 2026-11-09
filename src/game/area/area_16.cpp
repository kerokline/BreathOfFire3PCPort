// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_16.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using area_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]`: the index is not checked.
Handler Entry(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(address))))); }

// Area 16's own functions another of them calls directly, by their original
// addresses (each is patched to ours; the fuzz stands a recorder in).
constexpr ::bof3::image::CodeVa kFrameStep16{bof3::addr::Area16_FrameStep};

constexpr ::bof3::image::CodeVa kFrameHold16{bof3::addr::Area16_FrameHold};

constexpr ::bof3::image::CodeVa kBoxStep16{bof3::addr::Area16_BoxStep};

constexpr ::bof3::image::CodeVa kDrawFrame16{bof3::addr::Area16_DrawFrame};

constexpr ::bof3::image::CodeVa kDrawSprite16{bof3::addr::Area16_DrawSprite};

constexpr ::bof3::image::CodeVa kDrawHud16{bof3::addr::Area16_DrawHud};

using DrawAt = void (__cdecl*)(int, int);

using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

}  // namespace

// original 0x401B80 (WorldMap_FieldHooks 0x662DF0 entry 0; 0x56DE30 calls the
// entry of WorldMap_RecordIndex): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first of the nine 0x20-byte rows at 0x5E5E2C whose word is the
//      place 0x937F82, 9 when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript.
//      Any other: the cell's 4-byte record of 0x5E5C04 whose (x, z) bytes are
//      the leader's cell words (read again after the call; searched with NO
//      bound), its id byte +3; the name set of 0x5E5F4C whose id it is (3 when
//      none); up to four Text_Records rows from its item bytes (0xFF ends
//      them): "????????" and a 0 for an item whose byte at 0x9040EC is 0, else
//      the item's 16-byte name (0x669CD8 for item 0x16, else Item_NamePtr(0,
//      item + 0x38)); Msg_OpenScript(set + 0x16). Then 0x9039F4 (read again)
//      + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 3 (which reads the plate state table's bytes) is checked.
extern "C" void __cdecl Area16_PlaceMessage(void) {
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
    const auto z = static_cast<std::int16_t>(ObjTrio[0].z_whole);
    const auto x = static_cast<std::int16_t>(ObjTrio[0].x_whole);
    if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA1) {
        const unsigned place = AreaW0b_Place;
        std::int32_t row = 0;
        for (U a = ::bof3::Img(Area16_PlaceMessages_addr); a < ::bof3::Img(at::kA16PlaceMessagesEnd.v); a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(::bof3::Img(Area16_PlaceMessages_addr) + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = ObjTrio[0].x_whole;
        const unsigned cz = ObjTrio[0].z_whole;
        U cell = ::bof3::Img(Area16_Cells_addr);
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = ::bof3::Img(Area16_NameSets_addr); a < ::bof3::Img(at::kA16NameSetsEnd.v); a += 5, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(::bof3::Img(Area16_NameSets_addr) + 1 + set * 5);
        for (U row = ::bof3::Img(at::kTextRecords.v); row < ::bof3::Img(at::kTextRecordsEnd.v); row += 0x20, ++items) {
            const unsigned item = items[0];
            if (item == 0xFF) break;
            unsigned char* const out = At(row);
            if (At(::bof3::Img(at::kItemsHeld.v) + item)[0] == 0) {
                SetLong(out, 0x3F3F3F3F);
                SetLong(out + 4, 0x3F3F3F3F);
                out[8] = 0;
            } else {
                const unsigned char* const name =
                    item == 0x16 ? At(::bof3::Img(AreaW0b_ItemNameKey_addr)) : AH_CALL(Item_NamePtr)(0, (item + 0x38) & 0xFF);
                std::memcpy(out, name, 16);
            }
        }
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(set + 0x16));
    }
    const auto next = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
    Field_Request = 2;
    AreaW0c_TailState = next;
}

// original 0x401D00 (WorldMap_Records[0] +0; area 33's copy 0x403E00): the
// cell ahead of the leader - the high words of (leader +9) * (+0xC) + (+0x34)
// and of the same with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the
// kind +0xB by AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2,
// 0xAE 3, else Field_ScriptFlags2 bit 12 4, else 0; then Area16_PlateStates
// 0x5E5F5C by +1 (read after the calls, unchecked).
extern "C" void __cdecl Area16_PlateRun(void) {
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
    Entry(::bof3::Img(Area16_PlateStates_addr) + Cur()[1] * 4u)();
}

// original 0x401DE0 (Area16_PlateStates 0 and WorldMap33_PlateStates 0: one
// body for both maps): +0x24 = 0x80, Sprite_SetAnimationBank(0xF), then +0x29
// = 5, +0x2A, +0x5D, +0x5E, +0x5F = 0 and +1 = 1 (the show), Sprite_Current
// read again for each store.
extern "C" void __cdecl Area16_PlateStart(void) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(0xF);
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
}

// original 0x401E30 (Area16_PlateStates 1; area 33's 0x403EE0): +7 = +0xB.
// Kind 1: the entry of Area16_PlateAnims 0x5E5BE0 whose word is the place
// 0x937F82 - searched with NO bound - +0x18 = the place (zero-extended), its
// animation byte. Kinds 2, 3, 4: animations 3, 0, 1. Each: +0x40 = 0, +0x44 =
// 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation, +1 = 2. Other kinds: no more.
extern "C" void __cdecl Area16_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = AreaW0b_Place;
        U entry = ::bof3::Img(Area16_PlateAnims_addr);
        // DIV-0191: bounded at the table's end (Area16_Cells follows); a place
        // not in the nine entries shows no plate instead of reading on.
        const U end = g_mountain_passes ? ::bof3::Img(Area16_Cells_addr) : ~0u;
        while (entry < end && Word(At(entry)) != place) entry += 4;
        if (entry >= end) return;
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

// original 0x401F80 (Area16_PlateStates 2; area 33's 0x404030):
// WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0 +0x48 = 0 and +1 = 3;
// then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area16_PlateGrow(void) {
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

// original 0x401FD0 (Area16_PlateStates 3; area 33's 0x404080):
// WorldMap_PinSprite; unless Game_Mode is 1 the plate leaves (+0x48 = 2, +9 =
// 8, +1 = 4) when +0xB is not +7, or Field_Request is 5, or +7 is 1 and +0x18
// is not the place (zero-extended); a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area16_PlateHold(void) {
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

// original 0x402030 (Area16_PlateStates 4; area 33's 0x4040E0):
// WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0: a tail jump to
// Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to Effect_Release,
// else +0x48 = 0 and +1 = 1.
extern "C" void __cdecl Area16_PlateShrink(void) {
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

// original 0x402080 (record 0 +0xC; area 33's 0x404130): Area16_HudStates
// 0x5E5F70 by +1 - WorldMapHud_Start (shared), Area16_HudFrame.
extern "C" void __cdecl Area16_HudRun(void) { Entry(::bof3::Img(Area16_HudStates_addr) + Cur()[1] * 4u)(); }

// original 0x4020A0 (Area16_HudStates 1; area 33's 0x404150): `call
// 0x4020B0; jmp 0x402180` - the frame's slide, then the region box's.
extern "C" void __cdecl Area16_HudFrame(void) {
    area_harness::Phase(kFrameStep16.v)();
    area_harness::Phase(kBoxStep16.v)();
}

// original 0x4020B0 (area 33's WorldMap_FrameStep's dispatch): Area16_FrameStates
// 0x5E5F78 by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; the index unchecked.
extern "C" void __cdecl Area16_FrameStep(void) { Entry(::bof3::Img(Area16_FrameStates_addr) + Cur()[2] * 4u)(); }

// original 0x4020D0 (Area16_FrameStates 1): the word +0x2E += 0x10; at 0x10
// and above (s16) +2 + 1; then a tail jump to Area16_FrameHold.
extern "C" void __cdecl Area16_FrameSlideIn(void) {
    rv::Current<records::EffectWork>()->work_2e = static_cast<std::uint16_t>(rv::Current<records::EffectWork>()->work_2e) + 0x10u;
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(kFrameHold16.v)();
}

// original 0x402100 (Area16_FrameStates 2): the mode byte 2 makes +2 = 3;
// Area16_DrawFrame(0x10, the word +0x2E). The y pushed carries a stale high
// half the drawing never reads: ours passes the word sign-extended.
extern "C" void __cdecl Area16_FrameHold(void) {
    if (AreaW0b_MapMode == 2) Cur()[2] = 3;
    AH_AT(DrawAt, kDrawFrame16.v)(0x10, S16(Cur() + 0x2E));
}

// original 0x402130 (Area16_FrameStates 3): the word +0x2E -= 0x10; at -0x30
// and below +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0);
// Area16_DrawFrame(0x10, y).
extern "C" void __cdecl Area16_FrameSlideOut(void) {
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
    AH_AT(DrawAt, kDrawFrame16.v)(0x10, S16(o + 0x2E));
}

// original 0x402180 (area 33's WorldMapHud_BoxStep 0x404230): Area16_BoxStates
// 0x5E5F88 by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold,
// _BoxSlideOut.
extern "C" void __cdecl Area16_BoxStep(void) { Entry(::bof3::Img(Area16_BoxStates_addr) + Cur()[3] * 4u)(); }

// original 0x4021A0 (Area16_BoxStates 1; area 33's 0x404250): y +0x30 -= 10;
// at 0xC8 and below (s16) +3 + 1; when the box leaves +3 = 3;
// Area16_DrawHud(0x5C, y) - the y pushed with the object pointer's high half
// above the word, which the drawing never reads.
extern "C" void __cdecl Area16_BoxSlideIn(void) {
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
    AH_AT(DrawAt, kDrawHud16.v)(0x5C, S16(o + 0x30));
}

// original 0x402210 (Area16_BoxStates 2; area 33's 0x4042C0): while +0xB is 0
// +9 + 1, at 0x5A and above (u8) +0xB + 1; when the box leaves +3 + 1;
// Area16_DrawHud(0x5C, y).
extern "C" void __cdecl Area16_BoxHold(void) {
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
    AH_AT(DrawAt, kDrawHud16.v)(0x5C, S16(o + 0x30));
}

// original 0x402280 (Area16_BoxStates 3; area 33's 0x404330): y +0x30 += 10;
// at 0xF0 and above (s16) +3 = 0; unless the mode byte is set (whatever
// +0xB), Field_Request is 2 or Field_ScriptFlags bit 8, +3 = 1;
// Area16_DrawHud(0x5C, y).
extern "C" void __cdecl Area16_BoxSlideOut(void) {
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
    AH_AT(DrawAt, kDrawHud16.v)(0x5C, S16(o + 0x30));
}

// original 0x4022E0 (area 33's WorldMap_DrawFrame 0x404390, instruction for
// instruction; its tables 0x5E5FF0 / 0x5E5F98): the dial frame at (x, y),
// nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to slot 1; the dial
// (sprite 0) at (x, y); the cell under the leader (AreaMap_ByteAt of the high
// words of Field_Kind2X / Z); legend 1 at (x + 0x30, y) unless the cell is
// 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit 12, its key (the first of six
// button entries whose mask has a bit of the button word 0) at (x + 0x38, y +
// 8) as the entry's sprite + 1 (lit) or the sprite (withheld); legend 2 at (x
// + 0x30, y + 0x10) unless the cell is 0xA0 / 0xA1, its key from word 6 over
// EIGHT entries (the seventh and eighth are Area16_Record8States' words) at
// (x + 0x38, y + 0x10); legend 3 at (x + 0x30, y + 0x18) when
// Field_CellHasEvent(cell x, z) or flag bit 12, or the party set is 0xC, or
// Field_ScriptFlags bit 14; the needle at (x + 0x18, y + 0x18).
extern "C" void __cdecl Area16_DrawFrame(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    world_map_legend::Begin();   // DIV-0135: which page; the port's key names painted over
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_AT(DrawSpriteAt, kDrawSprite16.v)(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellX.v)))),
                                                       static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellZ.v)))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) AH_AT(DrawSpriteAt, kDrawSprite16.v)(x + 0x30, y, 1);
    world_map_legend::Key(AH_AT(DrawSpriteAt, kDrawSprite16.v), ::bof3::Img(Area16_Buttons_addr), 6, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000))), x + 0x38, y + 8,
                          withheld1);
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) AH_AT(DrawSpriteAt, kDrawSprite16.v)(x + 0x30, y + 0x10, 2);
    world_map_legend::Key(AH_AT(DrawSpriteAt, kDrawSprite16.v), ::bof3::Img(Area16_Buttons_addr), 8, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c))), x + 0x38, y + 0x10,
                          withheld2);
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellX.v)))),
                                            static_cast<short>(Word(At(::bof3::Img(at::kLeaderCellZ.v))))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (AreaW0b_PartySet & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) AH_AT(DrawSpriteAt, kDrawSprite16.v)(x + 0x30, y + 0x18, 3);
    world_map_legend::Start(x + 0x30, y + 0x18, !third);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// original 0x4024B0 (area 33's WorldMap_DrawSprite 0x404560; its table
// 0x5E5F98): one sprite of the dial page at (x, y): a draw-mode primitive
// committed to slot 1, then at Gfx_PacketNext (read again) a SPRT,
// semi-transparent when the index's low byte is not 0, colour 0x80 x 3, x
// and y as floats of the arguments' low words, CLUT 0x7B80, (w, h, u, v) from
// Area16_Sprites by index & 0xFF (unchecked); Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl Area16_DrawSprite(int x, int y, unsigned index) {
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
    const unsigned char* const entry = At(::bof3::Img(Area16_Sprites_addr) + (index & 0xFF) * 4u);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->w, entry[0]);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->h, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x402570 (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the region box (sprite 4) at (x, y), its cap (5) at
// (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low
// word of the dword 0x803588, read after the sprites).
extern "C" void __cdecl Area16_DrawHud(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, kDrawSprite16.v)(x, y, 4);
    AH_AT(DrawSpriteAt, kDrawSprite16.v)(x + 0x80, y, 5);
    const unsigned char* const text = At(::bof3::Img(at::kAreaText.v) + (static_cast<U>(Long(At(::bof3::Img(at::kAreaTextOffset.v)))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// original 0x4025D0 (record 0 +8): Area16_Record8States 0x5E6008 by +1 -
// 0x4253C0 and 0x40C490 (the world-map copies' shared states, not this
// group's) around Area16_Record8Place.
extern "C" void __cdecl Area16_Record8Run(void) { Entry(::bof3::Img(Area16_Record8States_addr) + Cur()[1] * 4u)(); }

// original 0x4025F0 (Area16_Record8States 1; area 33's copy 0x4046A0):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of Area16_Directions[d];
// +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words +0x36 /
// +0x3A less each direction word >> 13 (arithmetic, 16-bit), then +0x36 (when
// +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down otherwise. Then the
// words +0x36 / +0x3A less the direction words >> 9; +0xB = 0, +1 + 1;
// Sprite_SetAnimation(Area16_Record8Anims[d] byte 0), +0x2A = its byte 1 (d
// read again after the call); a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl Area16_Record8Place(void) {
    const auto dir = [](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(::bof3::Img(Area16_Directions_addr) + half + o[8] * 4u))); };
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
    AH_CALL(Sprite_SetAnimation)(At(::bof3::Img(Area16_Record8Anims_addr) + Cur()[8] * 2u)[0]);
    o = Cur();
    tO = rv::As<records::EffectObject>(o);
    o[0x2A] = At(::bof3::Img(Area16_Record8Anims_addr) + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x402750 (record 0 +4): Area16_Record4States 0x5E602C by +1 -
// Area16_Record4MarkCell, then 0x408990 (the copies' shared state, not this
// group's).
extern "C" void __cdecl Area16_Record4Run(void) { Entry(::bof3::Img(Area16_Record4States_addr) + Cur()[1] * 4u)(); }

// original 0x402770 (Area16_Record4States 0; area 33's copy 0x404820): with
// the byte 0x903A79 9 or Field_StatusBits bit 0, a tail jump to
// Effect_Release. Else WorldMap_RecordIndex (its answer not read),
// Sprite_SetAnimationBank(0x205); +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F =
// 0; the map byte of the cell record Area16_Cells[+0xB] (x, z; unchecked) set
// to 0xA0 - AreaMap_Bytes + AreaMap_Header[0] * z + x; Sprite_SetAnimation(0)
// and +1 = 1.
extern "C" void __cdecl Area16_Record4MarkCell(void) {
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
    const U record = ::bof3::Img(Area16_Cells_addr) + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// original 0x402830 (record 0 +0x10, through WorldMap_RecordHook10; area 33's
// WorldMap33_DrawDrift 0x4048E0 instruction for instruction, its table
// Area16_DriftUV 0x5E6034): docs/worldmap_area.md section 4. Once (+2 == 0):
// the word +0x3A += Frame_Counter & 0xF, +2 + 1. Nothing more unless
// Draw_PassFlags bit 2. With b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A
// = -8 above the map's height + 8; nothing more unless the leader is within
// 25 cells in x OR in z; one textured square through the GTE, then a grid of
// (17 - 4b) x (16 - 4b) semi-transparent map-item quads with u / v from the
// table by e (unchecked).
extern "C" void __cdecl Area16_DrawDrift(void) {
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
            const U m = At(::bof3::Img(Area16_DriftUV_addr) + 0x8 + static_cast<U>(e))[0];
            const U ubase = At(::bof3::Img(Area16_DriftUV_addr) + static_cast<U>(e))[0];
            const U vbase = At(::bof3::Img(Area16_DriftUV_addr) + 0x4 + static_cast<U>(e))[0];
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

void Area16_Inject() {
    BOF3_INJECT(Area16_PlaceMessage);
    BOF3_INJECT(Area16_PlateRun);
    BOF3_INJECT(Area16_PlateStart);
    BOF3_INJECT(Area16_PlateShow);
    BOF3_INJECT(Area16_PlateGrow);
    BOF3_INJECT(Area16_PlateHold);
    BOF3_INJECT(Area16_PlateShrink);
    BOF3_INJECT(Area16_HudRun);
    BOF3_INJECT(Area16_HudFrame);
    BOF3_INJECT(Area16_FrameStep);
    BOF3_INJECT(Area16_FrameSlideIn);
    BOF3_INJECT(Area16_FrameHold);
    BOF3_INJECT(Area16_FrameSlideOut);
    BOF3_INJECT(Area16_BoxStep);
    BOF3_INJECT(Area16_BoxSlideIn);
    BOF3_INJECT(Area16_BoxHold);
    BOF3_INJECT(Area16_BoxSlideOut);
    BOF3_INJECT(Area16_DrawFrame);
    BOF3_INJECT(Area16_DrawSprite);
    BOF3_INJECT(Area16_DrawHud);
    BOF3_INJECT(Area16_Record8Run);
    BOF3_INJECT(Area16_Record8Place);
    BOF3_INJECT(Area16_Record4Run);
    BOF3_INJECT(Area16_Record4MarkCell);
    BOF3_INJECT(Area16_DrawDrift);
}

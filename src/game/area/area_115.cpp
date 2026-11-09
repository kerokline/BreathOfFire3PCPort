// Moved here by tools/move_funcs.py from area_w2f.cpp, area_w3a.cpp (docs/source-layout-round.md).
#include "game/area/area_115.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area/area_115_callees.h"
#include "game/area_harness.h"
#include "game/area_w2f.h"
#include "game/area_w2f_callees.h"
#include "game/area_w3a.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_115::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }

void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }

using area_harness::Handler;

using at::WorldMapTables;

using move_script::At;

using move_script::SetLong;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
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
        for (U a = t.place_messages; a < t.place_messages_end; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(t.place_messages + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = ObjTrio[0].x_whole;
        const unsigned cz = ObjTrio[0].z_whole;
        U cell = t.cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = t.name_sets; a < t.name_sets_end; a += t.set_stride, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(t.name_sets + 1 + set * t.set_stride);
        for (U row = ::bof3::Img(at::kTextRecords.v); row < t.text_rows_end; row += 0x20, ++items) {
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

}  // namespace

// original 0x418960 (Field_ModeTailKinds[62], armed by Area113_Trigger63; a
// gap of the tool): the s8 state through the byte table 0x418A34 (12
// entries) to the jump table 0x418A1C (6), unsigned above 0xB: nothing.
//   0: the state = Area113_Reward's answer.
//   1: Field_Request not 2: ScriptFlags_Clear40, kind and state 0.
//   5: Field_Request not 2: Sound_PlayEffect(0x106), ScriptFlags_Clear40,
//      kind and state 0.
//   0xA: Field_Request not 2: Sound_StopMusic, Sound_LoadStream(2), state 0xB.
//   0xB: the stream done and Field_Request not 2: Sound_ResumeAll,
//      ScriptFlags_Clear40, kind and state 0, the focus object's word +0x88 =
//      9.
//   2..4, 6..9: nothing.
extern "C" void __cdecl Area113_TailReward(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0: B(::bof3::Img(AreaW0c_TailState_addr)) = AH_CALL(Area113_Reward)(); return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 5:
        if (Field_Request == 2) return;
        AH_CALL(Sound_PlayEffect)(0x106);
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 0xA:
        if (Field_Request == 2) return;
        AH_CALL(Sound_StopMusic)();
        AH_CALL(Sound_LoadStream)(2);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 0xB: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        if (Field_Request == 2) return;
        AH_CALL(Sound_ResumeAll)();
        AH_CALL(ScriptFlags_Clear40)();
        unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        SetWord(focus + 0x88, 9);
        return;
    }
    default: return;
    }
}

// original 0x418A40 (called by Area113_TailReward): by the byte 0x9045F4 -
//   0xA: story flag 0x96 set: message 5, answer 1; else Inventory_Add(3,
//        0x2D, 3), flag 0x96, message 2, answer 5.
//   0xB: flag 0x97 set: 5 / 1; else Inventory_Add(3, 0x33, 1), flag 0x97,
//        then flag 0x96 set: message 6, else Inventory_Add(3, 0x2D, 3), flag
//        0x96, message 3; answer 5.
//   0xC: key item 0xF held: 5 / 1; else KeyItem_Add(0xF), then flag 0x97
//        set: message 8; else item 0x33 and flag 0x97, then flag 0x96 set:
//        message 7, else item 0x2D and flag 0x96, message 4; answer 0xA.
//   Any other: message 1, answer 1.
// Then Msg_OpenScript(the message), Field_Request 2; al the answer. (The
// originals push a fourth word, 0, to Inventory_Add, which takes three.)
extern "C" unsigned char __cdecl Area113_Reward(void) {
    unsigned message = 1;
    unsigned char answer = 1;
    switch (B(::bof3::Img(FieldE1_Rank_addr))) {
    case 0xA:
        if (Test(0x96)) {
            message = 5;
            break;
        }
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 2;
        answer = 5;
        break;
    case 0xB:
        if (Test(0x97)) {
            message = 5;
            break;
        }
        AH_CALL(Inventory_Add)(3, 0x33, 1);
        Set(0x97);
        answer = 5;
        message = 6;
        if (Test(0x96)) break;
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 3;
        break;
    case 0xC:
        if (AH_CALL(KeyItem_Has)(0xF) != 0) {
            message = 5;
            break;
        }
        AH_CALL(KeyItem_Add)(0xF);
        answer = 0xA;
        message = 8;
        if (Test(0x97)) break;
        AH_CALL(Inventory_Add)(3, 0x33, 1);
        Set(0x97);
        message = 7;
        if (Test(0x96)) break;
        AH_CALL(Inventory_Add)(3, 0x2D, 3);
        Set(0x96);
        message = 4;
        break;
    default: break;
    }
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
    Field_Request = 2;
    return answer;
}

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

void Area115_Inject() {
    BOF3_INJECT(Area113_TailReward);
    BOF3_INJECT(Area113_Reward);
    BOF3_INJECT(Area115_PlaceMessage);
    BOF3_INJECT(Area115_PlateRun);
    BOF3_INJECT(Area115_PlateStart);
    BOF3_INJECT(Area115_PlateShow);
    BOF3_INJECT(Area115_PlateGrow);
    BOF3_INJECT(Area115_PlateHold);
    BOF3_INJECT(Area115_PlateShrink);
    BOF3_INJECT(Area115_HudRun);
}

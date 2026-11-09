// Moved here by tools/move_funcs.py from area_w0c.cpp, rest_3a.cpp, world_map.cpp, worldmap_area.cpp (docs/source-layout-round.md).
#include "game/area/world_map_33.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/area/world_map_33_callees.h"
#include "game/area_harness.h"
#include "game/area_w0c.h"
#include "game/area_w0c_callees.h"
#include "game/boss_harness.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a.h"
#include "game/rest_3a_callees.h"
#include "game/world_map.h"
#include "game/world_map_callees.h"
#include "game/worldmap_area.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = world_map_33::at;

}  // namespace

namespace {

using namespace world_map;

}  // namespace

using namespace worldmap_area;

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }

std::int16_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

using boss_harness::Phase;

short S16Rest3a(const unsigned char* p) { return static_cast<short>(Word(p)); }

// Sprite_Current, the running task slot (0x93B8C4) and its owner (0x93B940),
// each read afresh where the original reads it.
unsigned char* Sc() { return Sprite_Current; }

unsigned char* AtWorldMap(world_map::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

world_map::U WordWorldMap(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

world_map::U LongWorldMap(const unsigned char* p) {
    world_map::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

unsigned char* Object() { return AtWorldMap(LongWorldMap(AtWorldMap(world_map::at::Sprite_CurrentAt()))); }

// The map task's object, Object(), is an effect record: its frame runs from
// WorldMapHud_Frame, effect kind 0x58's handler, on the effect record.
unsigned char* PacketNext() { return AtWorldMap(LongWorldMap(AtWorldMap(world_map::at::Gfx_PacketNextAt()))); }

}  // namespace

namespace worldmap_area {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace worldmap_area

namespace {

using U = std::uint32_t;

// The object being run, re-read wherever the original re-reads it.
unsigned char* Cur() { return Sprite_Current; }

// Cur() is the effect record being run (every Cur() reader here is an effect
// kind's handler), typed as rv::As<records::EffectObject>(Cur()).
std::int32_t S16WorldmapArea(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]` do: the index is not checked.
Handler EntryWorldmapArea(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(worldmap_area::Long(worldmap_area::At(address))))); }

// The region box's leave test the HUD machine's states share (0x404274 ..
// 0x404297 and its copies): the map's mode byte set while the object's +0xB
// is, or Field_Request 2, or bit 8 of Field_ScriptFlags (read as a dword,
// `test ch, 1`).
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

}  // namespace

// original 0x403CE0 (area 33's handler 0, Area33_Handlers 0x5EF4C8 - the
// descriptor 0x5EF4D8's +0x3C; called by field-script ops 0x03 / 0xDE from
// 0x577B80; PSX 0x801F2C38): eight map cells to 0 through 0x579F00 (x, z,
// value) - x 0x3A and 0x3B of rows 0x1C, 0x1D, 0x1F and 0x20, in that order.
extern "C" void __cdecl Area33_ClearCellsA(void) {
    static constexpr unsigned char kRows[] = {0x1C, 0x1D, 0x1F, 0x20};
    for (const unsigned char z : kRows) {
        worldmap_area::g.set_cell(0x3A, z, 0);
        worldmap_area::g.set_cell(0x3B, z, 0);
    }
}

// original 0x403D40 (area 33's handler 1; PSX 0x801F2CD0): the cells (0x11,
// 0x15), (0x11, 0x16) and (0x12, 0x16) to 0.
extern "C" void __cdecl Area33_ClearCellsB(void) {
    worldmap_area::g.set_cell(0x11, 0x15, 0);
    worldmap_area::g.set_cell(0x11, 0x16, 0);
    worldmap_area::g.set_cell(0x12, 0x16, 0);
}

// original 0x403D70 (WorldMap_FieldHooks 0x662DF0 entry 1 - 0x56DE30 calls
// entry WorldMap_RecordIndex(); the map field frame's hook for the Yraall
// map): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the row of the six 0x20-byte rows at 0x5EF51C whose
//      first word is the place 0x937F82 (6 when none); message = the word at
//      0x5EF51C + (row * 16 + (s8) Cond_ByteFA) * 2; Msg_OpenScript of it;
//      0x9039F4 (read again) + 1, Field_Request = 2, then the state stored.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 (0x57C7A0) and the
//      three bytes 0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original has it: neither the row nor
// the byte is checked (row 6 reads the state table after the rows).
extern "C" void __cdecl WorldMap33_PlaceMessage(void) {
    const auto state = AreaW0c_TailState;
    if (state == 0) {
        worldmap_area::g.flags_set40();
        const unsigned place = AreaW0b_Place;
        std::int32_t row = 0;
        for (U a = ::bof3::Img(WorldMap33_PlaceMessages_addr); a < ::bof3::Img(worldmap_area::at::kPlaceMessagesEnd); a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + static_cast<signed char>(Cond_ByteFA);
        const unsigned message = Word(At(::bof3::Img(WorldMap33_PlaceMessages_addr) + static_cast<U>(index * 2)));
        worldmap_area::g.open_script(static_cast<unsigned short>(message));
        const auto next = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        Field_Request = 2;
        AreaW0c_TailState = next;
    } else if (state == 1) {
        if (Field_Request == 2) return;
        worldmap_area::g.flags_clear40();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        AreaW0c_TailSub = 0;
    }
}

// original 0x403E00: the cell ahead of the leader - x = the high word of
// (leader +9) * (+0xC) + (+0x34), z the same of +0x10 and +0x38 (dword
// products, u8 count) - to the object's +0xC / +0x10 (sign-extended), then
// its kind at +0xB: AreaMap_ByteAt(x, z) 0xA1 -> 1, else (asked again) 0xA0
// -> 2, else (again) 0xAE -> 3, else Field_ScriptFlags2 bit 12 -> 4, else 0.
// Then WorldMap33_PlateStates 0x5EF5DC by +1: 0x401DE0 (area 16's copy's
// state 0, shared), WorldMap33_PlateShow, _Grow, _Hold, _Shrink.
//
// As the original has it: the words pushed to AreaMap_ByteAt carry a stale
// stack word above them (it reads the low words); the cell is asked for up
// to three times.
extern "C" void __cdecl WorldMap33_PlateRun(void) {
    const U steps = ObjTrio[0].step_frames;
    const U xs = steps * static_cast<std::uint32_t>(ObjTrio[0].step_x) + static_cast<std::uint32_t>(ObjTrio[0].x);
    const U zs = steps * static_cast<std::uint32_t>(ObjTrio[0].step_z) + static_cast<std::uint32_t>(ObjTrio[0].z);
    const auto x = static_cast<short>(xs >> 16);
    const auto z = static_cast<short>(zs >> 16);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(Cur())->work_0c, x);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(Cur())->work_10, z);
    unsigned char kind;
    if (worldmap_area::g.byte_at(x, z) == 0xA1) kind = 1;
    else if (worldmap_area::g.byte_at(x, z) == 0xA0) kind = 2;
    else if (worldmap_area::g.byte_at(x, z) == 0xAE) kind = 3;
    else if ((Field_ScriptFlags2 & 0x1000) != 0) kind = 4;
    else kind = 0;
    Cur()[0xB] = kind;
    EntryWorldmapArea(::bof3::Img(WorldMap33_PlateStates_addr) + rv::As<records::EffectObject>(Cur())->state * 4u)();
}

// original 0x403EE0 (WorldMap33_PlateStates 1): +7 = +0xB (the kind shown).
// Kind 1: the entry of WorldMap33_PlateAnims 0x5EF320 whose word is the place
// 0x937F82 - searched with NO bound, as the original has it (the six entries'
// places are 0x17, 8, 7, 0xE, 0x13, 0x60) - +0x18 = the place (zero-
// extended), and its animation byte. Kinds 2, 3, 4: animations 3, 0, 1. For
// each of the four: +0x40 = 0, +0x44 = 0x10000, +0x48 = 2, +9 = 8,
// Sprite_SetAnimation(the animation), +1 = 2 (the grow). Kind 0 and any other
// do nothing more.
extern "C" void __cdecl WorldMap33_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = AreaW0b_Place;
        U entry = ::bof3::Img(WorldMap33_PlateAnims_addr);
        // DIV-0191: the original searches with no bound; a place not in the
        // table (one armed by a link cell this map has no plate for) runs
        // into the records after it and the animation byte is garbage
        // (the fault the owner hit on 2026-10-30). Ours stops at the table's
        // end, 0x5EF338 (WorldMap_Records record 1's data), and shows no plate.
        const U end = g_mountain_passes ? ::bof3::Img(WorldMap33_PlateAnims_addr) + 6 * 4 : ~0u;
        while (entry < end && Word(At(entry)) != place) entry += 4;
        if (entry >= end) return;
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(Cur())->work_18, static_cast<std::int32_t>(place));
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
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(Cur())->scale_x, 0);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(Cur())->scale_y, 0x10000);
    Cur()[0x48] = 2;
    rv::As<records::EffectObject>(Cur())->timer = 8;
    worldmap_area::g.set_animation(static_cast<unsigned char>(animation));
    rv::As<records::EffectObject>(Cur())->state = 2;
}

// original 0x404030 (WorldMap33_PlateStates 2): WorldMap_PinSprite; +0x40 +=
// 0x2000; +9 - 1, and at 0: +0x48 = 0, +1 = 3 (the hold); then a tail jump to
// Sprite_QueueOverlay. Eight frames from 0 to 0x10000.
extern "C" void __cdecl WorldMap33_PlateGrow(void) {
    worldmap_area::g.pin_sprite();
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(Cur())->scale_x, static_cast<std::int32_t>(static_cast<U>(rv::As<records::EffectObject>(Cur())->scale_x) + 0x2000));
    rv::As<records::EffectObject>(Cur())->timer = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->timer - 1);
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->timer == 0) {
        rv::Bytes(o)[0x48] = 0;
        rv::As<records::EffectObject>(Cur())->state = 3;
    }
    worldmap_area::g.queue_overlay();
}

// original 0x404080 (WorldMap33_PlateStates 3): WorldMap_PinSprite; unless
// Game_Mode is 1, the plate leaves (+0x48 = 2, +9 = 8, +1 = 4: the shrink)
// when the kind +0xB is not the one shown +7, or Field_Request is 5, or the
// kind shown is 1 and +0x18 is not the place 0x937F82 (zero-extended); then a
// tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl WorldMap33_PlateHold(void) {
    worldmap_area::g.pin_sprite();
    if (Game_Mode != 1) {
        unsigned char* const o = Cur();
        const unsigned shown = o[7];
        const bool leaves = o[0xB] != shown || Field_Request == 5 ||
                            (shown == 1 && static_cast<U>(rv::As<records::EffectObject>(o)->work_18) != AreaW0b_Place);
        if (leaves) {
            o[0x48] = 2;
            rv::As<records::EffectObject>(Cur())->timer = 8;
            rv::As<records::EffectObject>(Cur())->state = 4;
        }
    }
    worldmap_area::g.queue_overlay();
}

// original 0x4040E0 (WorldMap33_PlateStates 4): WorldMap_PinSprite; +0x40 -=
// 0x2000; +9 - 1. Not yet 0: a tail jump to Sprite_QueueOverlay. At 0: with
// Field_Request 5 a tail jump to Effect_Release, else +0x48 = 0 and +1 = 1
// (the show again), with no overlay that frame.
extern "C" void __cdecl WorldMap33_PlateShrink(void) {
    worldmap_area::g.pin_sprite();
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(Cur())->scale_x, static_cast<std::int32_t>(static_cast<U>(rv::As<records::EffectObject>(Cur())->scale_x) - 0x2000));
    rv::As<records::EffectObject>(Cur())->timer = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->timer - 1);
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->timer != 0) {
        worldmap_area::g.queue_overlay();
        return;
    }
    if (Field_Request == 5) {
        worldmap_area::g.effect_release();
        return;
    }
    rv::Bytes(o)[0x48] = 0;
    rv::As<records::EffectObject>(Cur())->state = 1;
}

// original 0x404130: WorldMapHud_States 0x5EF5F0 by +1 - WorldMapHud_Start
// (shared), WorldMapHud_Frame.
extern "C" void __cdecl WorldMapHud_Run(void) { EntryWorldmapArea(::bof3::Img(WorldMapHud_States_addr) + rv::As<records::EffectObject>(Cur())->state * 4u)(); }

// original 0x404150 (WorldMapHud_States 1): `call 0x404160; jmp 0x404230` -
// WorldMap_FrameStep (the dial's slide, byte +2), then WorldMapHud_BoxStep
// (the region box's, byte +3).
extern "C" void __cdecl WorldMapHud_Frame(void) {
    worldmap_area::g.frame_step();
    worldmap_area::g.box_step();
}

// original 0x404160 (the PSX 0x801F4178 family: a jalr through a table of the
// object's state byte): the map task's frame, called from 0x404150 before the
// HUD's state machine 0x404230. By byte +2 of Sprite_Current through the table
// 0x5EF5F8 (four entries; the byte is loaded whole and the original indexes
// past four into the HUD's table and the sprite table - never reached, and
// ours refuses it):
//   0: the shared block 0x411310 (Capcom's; eleven state machines share it):
//      unless the mode byte 0x9045FA is 2 or Field_ScriptFlags bit 8 is set,
//      y (the word at +0x2E) = -0x30 and the state 1;
//   1: y += 0x10; at 0x10 and above the state becomes 2; then as state 2;
//   2: the mode byte 2 makes the state 3; WorldMap_DrawFrame(0x10, y);
//   3: y -= 0x10; at -0x30 and below the state is 0; unless the mode byte is 2
//      the state is 1 (over the 0); WorldMap_DrawFrame(0x10, y).
// The y the original pushes carries the object pointer's (state 3) or a stale
// register's (state 2) high half above the word; every reader of the frame's
// y takes its low word (docs/world-map.md section 7.1), so ours passes the
// word sign-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_FrameStep() {
    records::EffectObject* object = rv::As<records::EffectObject>(Object());
    switch (object->step) {
    case 0:
        world_map::g.state0();
        return;
    case 1:
        object->screen_x = static_cast<U>(static_cast<std::uint16_t>(object->screen_x)) + 0x10;
        object = rv::As<records::EffectObject>(Object());
        if (static_cast<std::int32_t>(object->screen_x) >= 0x10) object->step = static_cast<unsigned char>(object->step + 1);
        [[fallthrough]];
    case 2:
        if (AreaW0b_MapMode == 2) {
            object = rv::As<records::EffectObject>(Object());
            object->step = 3;
        }
        object = rv::As<records::EffectObject>(Object());
        world_map::g.draw_frame(0x10, static_cast<std::int32_t>(object->screen_x));
        return;
    case 3:
        object->screen_x = static_cast<U>(static_cast<std::uint16_t>(object->screen_x)) - 0x10;
        object = rv::As<records::EffectObject>(Object());
        if (static_cast<std::int32_t>(object->screen_x) <= -0x30) {
            object->step = 0;
            object = rv::As<records::EffectObject>(Object());
        }
        if (AreaW0b_MapMode != 2) {
            object->step = 1;
            object = rv::As<records::EffectObject>(Object());
        }
        world_map::g.draw_frame(0x10, static_cast<std::int32_t>(object->screen_x));
        return;
    default:
        bof3::Fatal("WorldMap_FrameStep: the map task's state byte is %u, past its four-entry table", object->step);
    }
}

// original 0x404180 (WorldMap33_FrameStates[1]; the case block
// WorldMap_FrameStep carries inline): word +0x2E up 0x10; at 0x10 or more
// (signed, Sprite_Current read again) +2 up; then a tail jmp to state 2.
extern "C" void __cdecl WorldMap33_FrameSlideIn(void) {
    unsigned char* s = Sc();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x10);
    s = Sc();
    if (S16Rest3a(s + 0x2E) >= 0x10) s[2] = static_cast<unsigned char>(s[2] + 1);
    Phase(bof3::addr::WorldMap33_FrameShown)();
}

// original 0x4041B0 (WorldMap33_FrameStates[2], and the tail of 0x404180):
// the mode byte 0x9045FA 2 makes +2 = 3; WorldMap_DrawFrame(0x10, word
// +0x2E). The y is pushed in edx with a stale high half; every reader takes
// its low word (world_map.cpp), so ours passes the word sign-extended.
extern "C" void __cdecl WorldMap33_FrameShown(void) {
    if (AreaW0b_MapMode == 2) Sc()[2] = 3;
    const short y = S16Rest3a(Sc() + 0x2E);
    BH_CALL(WorldMap_DrawFrame)(0x10, y);
}

// original 0x4041E0 (WorldMap33_FrameStates[3]): word +0x2E down 0x10; at
// -0x30 or less +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0); then
// WorldMap_DrawFrame(0x10, word +0x2E) - pushed in eax, the high half the
// object pointer's.
extern "C" void __cdecl WorldMap33_FrameSlideOut(void) {
    unsigned char* s = Sc();
    SetWord(s + 0x2E, Word(s + 0x2E) - 0x10);
    s = Sc();
    if (S16Rest3a(s + 0x2E) <= -0x30) {
        s[2] = 0;
        s = Sc();
    }
    if (AreaW0b_MapMode != 2) {
        s[2] = 1;
        s = Sc();
    }
    BH_CALL(WorldMap_DrawFrame)(0x10, S16Rest3a(s + 0x2E));
}

// original 0x404230: WorldMapHud_BoxStates 0x5EF608 by +3 - WorldMapHud_BoxWait
// (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl WorldMapHud_BoxStep(void) { EntryWorldmapArea(::bof3::Img(WorldMapHud_BoxStates_addr) + Cur()[3] * 4u)(); }

// original 0x404250 (WorldMapHud_BoxStates 1): y +0x30 -= 10; at 0xC8 and
// below (s16) +3 + 1; then when the box leaves +3 = 3 (over the + 1);
// WorldMap_DrawHud(0x5C, y).
//
// As the original has it: the y pushed is the word in a register whose upper
// half is the object pointer's; WorldMap_DrawHud and what it calls read the
// low word (docs/world-map-hud.md section 1.4), so ours passes the word
// sign-extended.
extern "C" void __cdecl WorldMapHud_BoxSlideIn(void) {
    rv::As<records::EffectObject>(Cur())->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_y) - 10u;
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (static_cast<std::int32_t>(o->screen_y) <= 0xC8) {
        rv::Bytes(o)[3] = static_cast<unsigned char>(rv::Bytes(o)[3] + 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    if (BoxLeaves(rv::Bytes(o))) {
        rv::Bytes(o)[3] = 3;
        o = rv::As<records::EffectObject>(Cur());
    }
    worldmap_area::g.draw_hud(0x5C, static_cast<std::int32_t>(o->screen_y));
}

// original 0x4042C0 (WorldMapHud_BoxStates 2): while +0xB is 0 the counter
// +9 + 1, and at 0x5A and above (u8) +0xB + 1; then when the box leaves +3 +
// 1; WorldMap_DrawHud(0x5C, y).
extern "C" void __cdecl WorldMapHud_BoxHold(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (rv::Bytes(o)[0xB] == 0) {
        o->timer = static_cast<unsigned char>(o->timer + 1);
        o = rv::As<records::EffectObject>(Cur());
        if (o->timer >= 0x5A) {
            rv::Bytes(o)[0xB] = static_cast<unsigned char>(rv::Bytes(o)[0xB] + 1);
            o = rv::As<records::EffectObject>(Cur());
        }
    }
    if (BoxLeaves(rv::Bytes(o))) {
        rv::Bytes(o)[3] = static_cast<unsigned char>(rv::Bytes(o)[3] + 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    worldmap_area::g.draw_hud(0x5C, static_cast<std::int32_t>(o->screen_y));
}

// original 0x404330 (WorldMapHud_BoxStates 3): y +0x30 += 10; at 0xF0 and
// above (s16) +3 = 0; then unless the mode byte is set (whatever +0xB), or
// Field_Request is 2, or Field_ScriptFlags bit 8, +3 = 1 (over the 0);
// WorldMap_DrawHud(0x5C, y).
extern "C" void __cdecl WorldMapHud_BoxSlideOut(void) {
    rv::As<records::EffectObject>(Cur())->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_y) + 10u;
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (static_cast<std::int32_t>(o->screen_y) >= 0xF0) {
        rv::Bytes(o)[3] = 0;
        o = rv::As<records::EffectObject>(Cur());
    }
    if (AreaW0b_MapMode == 0 && Field_Request != 2 && (Field_ScriptFlags & 0x100) == 0) {
        rv::Bytes(o)[3] = 1;
        o = rv::As<records::EffectObject>(Cur());
    }
    worldmap_area::g.draw_hud(0x5C, static_cast<std::int32_t>(o->screen_y));
}

// original 0x404390 (PSX 0x801F3B00, branch for branch): the map's frame at
// (x, y), nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0): tpage 0x9C, 8-bit at (768, 256),
// blend mode 0; the PSX picks 0x22C on GetGraphType 1 or 2) committed to slot
// 1; the dial at (x, y); the cell under the leader (AreaMap_ByteAt of the
// high words of Field_Kind2X, Field_Kind2Z). Then three legend rows:
//   1 at (x + 0x30, y), unless the cell is 0xA0, 0xA1 or 0xAE or
//     Field_ScriptFlags2 bit 12: its key - the first of the six button-table
//     entries whose mask has a bit of the button map's word 0 - drawn at
//     (x + 0x38, y + 8) as the entry's sprite index + 1 (the lit glyph), or,
//     when the label is withheld, as the index itself (the dim one);
//   2 at (x + 0x30, y + 0x10) unless the cell is 0xA0 or 0xA1: its key from
//     word 6 over EIGHT entries (the seventh and eighth are the state table's
//     words 0x5EF688 / 0x5EF68C - kept), the same lit / dim rule at
//     (x + 0x38, y + 0x10);
//   3 at (x + 0x30, y + 0x18) when 0x531920(cell x, cell z) says so, or flag
//     bit 12, or the party set (0x90412C & 0x7F) is 0xC, or Field_ScriptFlags
//     bit 14;
// and the needle at (x + 0x18, y + 0x18). The flags are read after the calls
// that precede them; the index the original pushes for a lit key carries the
// button word's upper bytes, which the sprite draw masks off.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawFrame(int x, int y) {
    if (!(AtWorldMap(world_map::at::Draw_PassFlagsAt())[0] & 0x1B)) return;
    world_map_legend::Begin();   // DIV-0135: which page; the port's key names painted over
    world_map::g.set_draw_mode(PacketNext(), 0, 0, 0x9C, 0);
    world_map::g.commit_prim(1, 0xC);
    world_map::g.draw_sprite(x, y, 0);
    const unsigned char cell =
        world_map::g.byte_at(static_cast<short>(WordWorldMap(AtWorldMap(::bof3::Img(kLeaderCellX)))), static_cast<short>(WordWorldMap(AtWorldMap(::bof3::Img(kLeaderCellZ)))));
    const bool withheld1 =
        cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (WordWorldMap(AtWorldMap(world_map::at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!withheld1) world_map::g.draw_sprite(x + 0x30, y, 1);
    world_map_legend::Key(world_map::g.draw_sprite, ::bof3::Img(kButtonTable), 6, LongWorldMap(AtWorldMap(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000)), x + 0x38, y + 8, withheld1);
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) world_map::g.draw_sprite(x + 0x30, y + 0x10, 2);
    world_map_legend::Key(world_map::g.draw_sprite, ::bof3::Img(kButtonTable), 8, LongWorldMap(AtWorldMap(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c)), x + 0x38, y + 0x10, withheld2);
    const unsigned char event =
        world_map::g.cell_event(static_cast<short>(WordWorldMap(AtWorldMap(::bof3::Img(kLeaderCellX)))), static_cast<short>(WordWorldMap(AtWorldMap(::bof3::Img(kLeaderCellZ)))));
    bool third = event != 0;
    if (!third) third = (WordWorldMap(AtWorldMap(world_map::at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!third) third = (AreaW0b_PartySet & 0x7F) == 0xC;
    if (!third) third = (WordWorldMap(AtWorldMap(world_map::at::Field_ScriptFlagsAt())) & 0x4000) != 0;
    if (third) world_map::g.draw_sprite(x + 0x30, y + 0x18, 3);
    world_map_legend::Start(x + 0x30, y + 0x18, !third);
    world_map::g.draw_needle(x + 0x18, y + 0x18);
}

extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawSprite(int x, int y, unsigned index) {
    world_map::g.set_draw_mode(PacketNext(), 0, 0, 0x9C, 0);
    world_map::g.commit_prim(1, 0xC);
    unsigned char* const prim = PacketNext();
    world_map::g.set_sprt(prim);
    world_map::g.set_semi_trans(prim, (world_map_dial::g_dial_semi != 0 || (index & 0xFF) != 0) ? 1u : 0u);
    PutFloat(prim + 8, static_cast<float>(static_cast<short>(x)));
    PutFloat(prim + 0xC, static_cast<float>(static_cast<short>(y)));
    prim[4] = prim[5] = prim[6] = 0x80;
    rv::As<records::GpuSprt>(prim)->clut = 0x7B80;
    const unsigned char* const entry = AtWorldMap(::bof3::Img(kSpriteTable) + (index & 0xFF) * 4u);
    rv::As<records::GpuSprt>(prim)->w = entry[0];
    rv::As<records::GpuSprt>(prim)->h = entry[1];
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    world_map::g.commit_prim(1, 0x1C);
}

// original 0x404620 (PSX 0x801F40C4): the HUD at (x, y), nothing unless
// Draw_PassFlags & 0x1B: the region box (sprite 4, 128 x 24) at (x, y), its
// cap (sprite 5, 8 x 24) at (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0,
// 0xFF, text) with the region's name at 0x803580 + the low word of the dword
// 0x803588 - the area's 0x80010000 section's word +8, read after the two
// sprites. The return (Text_DrawAt's) is not read by the three callers.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawHud(int x, int y) {
    if (!(AtWorldMap(world_map::at::Draw_PassFlagsAt())[0] & 0x1B)) return;
    world_map::g.draw_sprite(x, y, 4);
    world_map::g.draw_sprite(x + 0x80, y, 5);
    const unsigned char* text = AtWorldMap(::bof3::Img(kAreaText) + (LongWorldMap(AtWorldMap(::bof3::Img(kAreaTextOffset))) & 0xFFFF));
    world_map::g.text_draw_at(x + 4, y + 4, 0, 0xFF, text);
}

// original 0x404680 (WorldMap_Records[1] +8): WorldMap33_Record08States by
// Sprite_Current[1] - 0x4253C0 (shared by ten world maps), Area33_Record08Start,
// 0x40C490 (shared).
extern "C" void __cdecl Area33_Record08Run(void) {
    StateEntry("Area33_Record08Run", ::bof3::Img(WorldMap33_Record08States_addr), area_w0c::at::kRecord08StateCount, Sprite_Current[1])();
}

// original 0x4046A0 (WorldMap33_Record08States[1]): animation bank 0x46;
// +0x48, +0x24 = 0, +0x29 = 5; the step words of WorldMap33_Record08Steps by
// the direction +8 to +0xC / +0x10 (sign-extended); the leader's position
// +0x34 / +0x38; for a variant +6 not 0, the words +0x36 / +0x3A drawn back
// by the step >> 13 and nudged 2 along one axis (+6 == 1: forward, else
// back; the axis +0x36 when +0xC is 0); always drawn back by the step >> 9;
// +0xB = 0, state + 1; the animation and +0x2A from WorldMap33_Record08Anims;
// then Sprite_UpdateScreen. The direction indexes both tables unchecked, as
// the original's (reads on through .data).
extern "C" void __cdecl Area33_Record08Start(void) {
    AH_CALL(Sprite_SetAnimationBank)(0x46);
    unsigned char* cur = Sprite_Current;
    auto* tCur = rv::As<records::EffectObject>(cur);
    cur[0x48] = 0;
    cur[0x24] = 0;
    cur[0x29] = 5;
    const auto StepX = [&] { return S16(Mem(::bof3::Img(WorldMap33_Record08Steps_addr) + cur[8] * 4u)); };
    const auto StepZ = [&] { return S16(Mem(::bof3::Img(WorldMap33_Record08Steps_addr) + 2 + cur[8] * 4u)); };
    tCur->step_x = StepX();
    tCur->step_z = StepZ();
    tCur->x = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
    tCur->z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
    if (cur[6] != 0) {
        AddWord(cur + 0x36, -static_cast<std::int16_t>(StepX() >> 13));
        AddWord(cur + 0x3A, -static_cast<std::int16_t>(StepZ() >> 13));
        const int nudge = cur[6] == 1 ? 2 : -2;
        if (tCur->step_x == 0) AddWord(cur + 0x36, nudge);
        else AddWord(cur + 0x3A, nudge);
    }
    AddWord(cur + 0x36, -static_cast<std::int16_t>(StepX() >> 9));
    AddWord(cur + 0x3A, -static_cast<std::int16_t>(StepZ() >> 9));
    cur[0xB] = 0;
    cur[1] = static_cast<unsigned char>(cur[1] + 1);
    AH_CALL(Sprite_SetAnimation)(B(::bof3::Img(WorldMap33_Record08Anims_addr) + cur[8] * 2u));
    cur = Sprite_Current;
    tCur = rv::As<records::EffectObject>(cur);
    cur[0x2A] = B(::bof3::Img(WorldMap33_Record08Anims_addr) + 1 + cur[8] * 2u);
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x404800 (WorldMap_Records[1] +4): WorldMap33_Record04States by
// Sprite_Current[1] - Area33_Record04Start, 0x408990 (shared by ten world
// maps).
extern "C" void __cdecl Area33_Record04Run(void) {
    StateEntry("Area33_Record04Run", ::bof3::Img(WorldMap33_Record04States_addr), area_w0c::at::kRecord04StateCount, Sprite_Current[1])();
}

// original 0x404820 (WorldMap33_Record04States[0]): with CharacterRecords
// record 0's byte +9 at 9, or Field_StatusBits bit 0, Effect_Release.
// Otherwise WorldMap_RecordIndex (its answer unused), animation bank 0x205,
// +0x48, +0x24, +0x2A and the tint bytes 0; the map cell of
// WorldMap33_Record04Cells[+0xB] (x, z) set to 0xA0 (AreaMap_Bytes + z *
// the header's width byte + x, unchecked); Sprite_SetAnimation(0); state 1.
extern "C" void __cdecl Area33_Record04Start(void) {
    if (B(::bof3::Img(CharacterRecords_addr) + 0x9) == 9 || (Field_StatusBits & 1) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    AH_CALL(WorldMap_RecordIndex)();
    AH_CALL(Sprite_SetAnimationBank)(0x205);
    unsigned char* const cur = Sprite_Current;
    cur[0x48] = 0;
    cur[0x24] = 0;
    cur[0x2A] = 0;
    cur[0x5D] = 0;
    cur[0x5E] = 0;
    cur[0x5F] = 0;
    const unsigned width = AreaMap_Header[0];
    const std::uint32_t cell = ::bof3::Img(WorldMap33_Record04Cells_addr) + cur[0xB] * 4u;
    const unsigned z = B(cell + 1), x = B(cell);
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[1] = 1;
}

// original 0x4048E0. Once (+2 == 0): +0x3A += Frame_Counter & 0xF (a word),
// +2 + 1. Nothing more unless Draw_PassFlags bit 2. With b = +0xB (u8) and e
// = b - 2:
//   - the dword +0x38 += b << 10; the word +0x3A = -8 when (s16) above the
//     map's height AreaMap_Header[1] + 8;
//   - nothing more unless |leader cell x - (s16) +0x36| <= 25 OR |leader cell
//     z - (s16) +0x3A| <= 25 (the high words of Field_Kind2X / Z);
//   - one POLY_FT4 at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0); the
//     square of half-side r = |15 - (Frame_Counter & 0x1F)| + ((4 - b) << 8)
//     about ((+0x34 >> 9) - 0x3FC0, (+0x38 >> 9) - 0x3FC0), its four
//     SVECTORs (x, z, -0x300) into Prim_VertexScratch (pads untouched),
//     Gte_RotTransPers4 into the corners with two locals (ten arguments),
//     Gte_PrimDepths4_10, Prim_SetTexture(e | 0xBB509100, prim, 1),
//     Gfx_CommitPrim(4, 0x48);
//   - then (17 - 4b) rows j of (16 - 4b) columns i: for each map item
//     MapView_ItemHalfAt(+0x36 + i + b - 4, +0x3A + j + b - 4) answers, a
//     semi-transparent POLY_FT4 at Gfx_PacketNext (read again) with the
//     item's four corners, colour 0x28, CLUT 0x78CB, tpage 0x5B, and u / v
//     bytes from WorldMap33_DriftUV by e (u base, v base, cell size m): u = i
//     * m + u base (i + 1 for corners 1, 3), v = j * m - ((u16 +0x38 * m) >>
//     16) + v base (j + 1 for corners 2, 3); MapView_LinkPrimAt(x << 16, z
//     << 16, 1, 0x48) with the item's cell read again.
//
// As the original has it: e indexes the u / v bytes unchecked (b 2 and 3 are
// the table's; b 0, 1 or 4 read its neighbours); the byte products wrap; the
// distance test is an OR (docs/worldmap_area.md section 4).
extern "C" void __cdecl WorldMap33_DrawDrift(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (o->step == 0) {
        o->z_whole = o->z_whole + (Frame_Counter & 0xF);
        rv::As<records::EffectObject>(Cur())->step = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->step + 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const std::int32_t b = rv::Bytes(o)[0xB];
    const std::int32_t e = b - 2;
    o->z = static_cast<std::int32_t>(static_cast<U>(o->z) + (static_cast<U>(b) << 10));
    o = rv::As<records::EffectObject>(Cur());
    if (S16WorldmapArea(rv::Bytes(o) + 0x3A) > AreaMap_Header[1] + 8) {
        o->z_whole = 0xFFF8;
        o = rv::As<records::EffectObject>(Cur());
    }
    std::int32_t dx = S16WorldmapArea(At(::bof3::Img(worldmap_area::at::kLeaderCellX))) - S16WorldmapArea(rv::Bytes(o) + 0x36);
    if (dx < 0) dx = -dx;
    if (dx > 0x19) {
        std::int32_t dz = S16WorldmapArea(At(::bof3::Img(worldmap_area::at::kLeaderCellZ))) - S16WorldmapArea(rv::Bytes(o) + 0x3A);
        if (dz < 0) dz = -dz;
        if (dz > 0x19) return;
    }

    unsigned char* const prim = Gfx_PacketNext;
    worldmap_area::g.set_poly_ft4(prim);
    worldmap_area::g.set_shade_tex(prim, 0);
    o = rv::As<records::EffectObject>(Cur());
    const std::int32_t side = 2 - e;   // 4 - b
    std::int32_t r = 15 - static_cast<std::int32_t>(Frame_Counter & 0x1F);
    if (r < 0) r = -r;
    r += static_cast<std::int32_t>(static_cast<U>(side) << 8);
    const std::int32_t cx = (o->x >> 9) - 0x3FC0;
    const std::int32_t cz = (o->z >> 9) - 0x3FC0;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    rv::As<records::SVector>(v)[0].z = 0xFD00;
    rv::As<records::SVector>(v)[2].y = static_cast<unsigned>(cz + r);
    rv::As<records::SVector>(v)[3].y = static_cast<unsigned>(cz + r);
    rv::As<records::SVector>(v)[1].x = static_cast<unsigned>(cx + r);
    rv::As<records::SVector>(v)[3].x = static_cast<unsigned>(cx + r);
    rv::As<records::SVector>(v)[0].x = static_cast<unsigned>(cx - r);
    rv::As<records::SVector>(v)[2].x = static_cast<unsigned>(cx - r);
    rv::As<records::SVector>(v)[0].y = static_cast<unsigned>(cz - r);
    rv::As<records::SVector>(v)[1].y = static_cast<unsigned>(cz - r);
    rv::As<records::SVector>(v)[1].z = 0xFD00;
    rv::As<records::SVector>(v)[2].z = 0xFD00;
    rv::As<records::SVector>(v)[3].z = 0xFD00;
    long depth = 0, flag = 0;
    const auto* const sv = reinterpret_cast<const short*>(v);
    worldmap_area::g.rot_trans_pers4(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                      reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    worldmap_area::g.prim_depths(prim);
    worldmap_area::g.set_texture(static_cast<U>(e) | 0xBB509100u, prim, 1);
    worldmap_area::g.commit_prim(4, 0x48);

    const std::int32_t rows = 9 - 4 * e;   // 17 - 4b
    const std::int32_t columns = 4 * side; // 16 - 4b
    for (std::int32_t j = 0; j < rows; ++j) {
        for (std::int32_t i = 0; i < columns; ++i) {
            o = rv::As<records::EffectObject>(Cur());
            unsigned char* const item = worldmap_area::g.item_half_at(S16WorldmapArea(rv::Bytes(o) + 0x36) + i + e - 2, S16WorldmapArea(rv::Bytes(o) + 0x3A) + j + e - 2);
            if (item == nullptr) continue;
            unsigned char* const q = Gfx_PacketNext;
            worldmap_area::g.set_poly_ft4(q);
            worldmap_area::g.set_shade_tex(q, 0);
            worldmap_area::g.set_semi_trans(q, 1);
            std::memcpy(q + 0x08, item + 0x08, 12);
            std::memcpy(q + 0x18, item + 0x18, 12);
            std::memcpy(q + 0x28, item + 0x28, 12);
            std::memcpy(q + 0x38, item + 0x38, 12);
            SetWord(q + 0x16, 0x78CB);
            SetWord(q + 0x26, 0x5B);
            q[4] = q[5] = q[6] = 0x28;
            const U m = At(::bof3::Img(WorldMap33_DriftUV_addr) + 0x8 + static_cast<U>(e))[0];
            const U ubase = At(::bof3::Img(WorldMap33_DriftUV_addr) + static_cast<U>(e))[0];
            const U vbase = At(::bof3::Img(WorldMap33_DriftUV_addr) + 0x4 + static_cast<U>(e))[0];
            const U scroll = (rv::As<records::EffectObject>(Cur())->z_frac * m) >> 16;
            const U ui = static_cast<U>(i), uj = static_cast<U>(j);
            q[0x14] = static_cast<unsigned char>(ui * m + ubase);
            q[0x15] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x24] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x25] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x34] = static_cast<unsigned char>(ui * m + ubase);
            q[0x35] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            q[0x44] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x45] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            o = rv::As<records::EffectObject>(Cur());
            const U x = static_cast<U>(S16WorldmapArea(rv::Bytes(o) + 0x36) + i + e - 2) << 16;
            const U z = static_cast<U>(S16WorldmapArea(rv::Bytes(o) + 0x3A) + j + e - 2) << 16;
            worldmap_area::g.link_prim_at(x, z, 1, 0x48);
        }
    }
}

void WorldMap33_Inject() {
    BOF3_INJECT(Area33_ClearCellsA);
    BOF3_INJECT(Area33_ClearCellsB);
    BOF3_INJECT(WorldMap33_PlaceMessage);
    BOF3_INJECT(WorldMap33_PlateRun);
    BOF3_INJECT(WorldMap33_PlateShow);
    BOF3_INJECT(WorldMap33_PlateGrow);
    BOF3_INJECT(WorldMap33_PlateHold);
    BOF3_INJECT(WorldMap33_PlateShrink);
    BOF3_INJECT(WorldMapHud_Run);
    BOF3_INJECT(WorldMapHud_Frame);
    BOF3_INJECT(WorldMap_FrameStep);
    BOF3_INJECT(WorldMap33_FrameSlideIn);
    BOF3_INJECT(WorldMap33_FrameShown);
    BOF3_INJECT(WorldMap33_FrameSlideOut);
    BOF3_INJECT(WorldMapHud_BoxStep);
    BOF3_INJECT(WorldMapHud_BoxSlideIn);
    BOF3_INJECT(WorldMapHud_BoxHold);
    BOF3_INJECT(WorldMapHud_BoxSlideOut);
    BOF3_INJECT(WorldMap_DrawFrame);
    BOF3_INJECT(WorldMap_DrawSprite);
    BOF3_INJECT(WorldMap_DrawHud);
    BOF3_INJECT(Area33_Record08Run);
    BOF3_INJECT(Area33_Record08Start);
    BOF3_INJECT(Area33_Record04Run);
    BOF3_INJECT(Area33_Record04Start);
    BOF3_INJECT(WorldMap33_DrawDrift);
}

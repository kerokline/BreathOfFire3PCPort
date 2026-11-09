// Moved here by tools/move_funcs.py from area_w1b.cpp, world_map.cpp (docs/source-layout-round.md).
#include "game/area/area_45.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b.h"
#include "game/area_w1b_callees.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "game/world_map_callees.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1b::at;

}  // namespace

namespace {

using namespace world_map;

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

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char& B(U address) { return move_script::At(address)[0]; }

signed char TailState() { return AreaW0c_TailState; }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

constexpr ::bof3::image::CodeVa kSetGates44{bof3::addr::Area44_SetGates};

constexpr ::bof3::image::CodeVa kPushParty44{bof3::addr::Area44_PushParty};

using PushPartyFn = void (__cdecl*)(unsigned, unsigned);

// Area 45's (area 16's copy's) own functions another of them calls directly.
constexpr ::bof3::image::CodeVa kFrameStep45{bof3::addr::Area45_FrameStep};

constexpr ::bof3::image::CodeVa kFrameHold45{bof3::addr::Area45_FrameHold};

constexpr ::bof3::image::CodeVa kBoxStep45{bof3::addr::Area45_BoxStep};

constexpr ::bof3::image::CodeVa kDrawFrame45{bof3::addr::Area45_DrawFrame};

constexpr ::bof3::image::CodeVa kDrawSprite45{bof3::addr::Area45_DrawSprite};

constexpr ::bof3::image::CodeVa kDrawHud45{bof3::addr::Area45_DrawHud};

using DrawAt = void (__cdecl*)(int, int);

using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

// The gate byte 0x90384B: gate A in bits 0..1, gate B in bits 4..5.
unsigned GateA() { return B(::bof3::Img(at::kCells44.v)) & 3u; }

unsigned GateB() { return (B(::bof3::Img(at::kCells44.v)) >> 4) & 3u; }

void SetByte(unsigned x, unsigned z, unsigned v) { AH_CALL(AreaMap_SetByte)(x, z, v); }

unsigned char* AtWorldMap(world_map::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

world_map::U LongWorldMap(const unsigned char* p) {
    world_map::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// The map task's object, Object(), is an effect record: its frame runs from
// WorldMapHud_Frame, effect kind 0x58's handler, on the effect record.
unsigned char* PacketNext() { return AtWorldMap(LongWorldMap(AtWorldMap(world_map::at::Gfx_PacketNextAt()))); }

// The needle's corner arithmetic, as the original's x87 executes it: the
// integer offset loaded with fild, the corner's float loaded over it, the
// subtraction at the control word's precision, one store to a float.
//   fild d; fld a; fsub st(1); fstp a
inline float X87SubInt(float a, std::int32_t d) {
    float r;
    __asm__ volatile(
        "fildl %2\n\t"
        "flds %1\n\t"
        "fsub %%st(1), %%st\n\t"
        "fstps %0\n\t"
        "fstp %%st(0)"
        : "=m"(r)
        : "m"(a), "m"(d)
        : "st", "st(1)");
    return r;
}

float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

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
        B(::bof3::Img(area_w1b::at::kCount44.v)) = 0;
        B(::bof3::Img(area_w1b::at::kCells44.v)) = 0;
        AH_CALL(Flags_Clear)(Bank(), 0x16);
        AH_CALL(Flags_Clear)(Bank(), 0x17);
        return;
    }
    if (fd != 1) return;
    const unsigned a = GateA(), b = GateB();
    unsigned char* const e0 = At(::bof3::Img(Sprite_ObjectsExtra_addr));
    unsigned char* const e1 = At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0xa4);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e0)->x_frac, 0x8000);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e0)->z_frac, 0x8000);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e1)->x_frac, 0x8000);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e0)->x_whole, At(::bof3::Img(Area44_Gate0_addr) + a * 2)[0]);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e0)->z_whole, At(::bof3::Img(Area44_Gate0_addr) + a * 2 + 1)[0]);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e0)->height_whole, 0x80);
    e0[0x80] = static_cast<unsigned char>(e0[0x80] | 0x80);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e1)->z_frac, 0x8000);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e1)->height_whole, 0x80);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e1)->x_whole, At(::bof3::Img(Area44_Gate1_addr) + b * 2)[0]);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(e1)->z_whole, At(::bof3::Img(Area44_Gate1_addr) + b * 2 + 1)[0]);
    e1[0x80] = static_cast<unsigned char>(e1[0x80] | 0x80);
    area_harness::Phase(kSetGates44.v)();
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
    const unsigned char facing = ObjTrio[0].direction;
    U rec = ::bof3::Img(Area44_Switches_addr);
    for (; rec < ::bof3::Img(area_w1b::at::kA44SwitchesEnd.v); rec += 5) {
        const unsigned char* const r = At(rec);
        if (r[0] == static_cast<unsigned char>(x) && r[1] == static_cast<unsigned char>(z) && (r[2] & 0xF) == facing) break;
    }
    if (rec >= ::bof3::Img(area_w1b::at::kA44SwitchesEnd.v)) return 0;
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), area_w1b::kFlagsToggle.v)(Bank(), At(rec)[3]);
    AH_CALL(Sound_PlayEffect)(0x200);
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = At(rec)[4];
    AreaW0c_TailState = 0;
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
                AH_AT(PushPartyFn, kPushParty44.v)(3, 0);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>((B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF1) | 1);
                AreaW0c_TailState = 1;
                return;
            }
        } else if (a == 1) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) == 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(7, 0);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>(B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF0);
                AreaW0c_TailState = 1;
                return;
            }
        }
        AreaW0c_TailState = 0xA;
        return;
    case 1:
        if (Sprite_ObjectsExtra[0].step_frames != 0) return;
        area_harness::Phase(kSetGates44.v)();
        [[fallthrough]];
    case 0xA:
        if (b == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) != 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(1, 1);
                AH_CALL(Sound_PlayEffect)(static_cast<unsigned char>(AreaW0c_TailState) == 1 ? 0x20E : 0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>((B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF) | 0x10);
                AreaW0c_TailState = 0xB;
                return;
            }
        } else if (b == 1) {
            if (AH_CALL(Flags_Test)(Bank(), 0x16) == 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(5, 1);
                AH_CALL(Sound_PlayEffect)(static_cast<unsigned char>(AreaW0c_TailState) == 1 ? 0x20E : 0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>(B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF);
                AreaW0c_TailState = 0xB;
                return;
            }
        }
        if (static_cast<unsigned char>(AreaW0c_TailState) == 0xB) return;
        if (static_cast<unsigned char>(AreaW0c_TailState) == 1) AH_CALL(Sound_PlayEffect)(0x202);
        AreaW0c_TailState = 0x14;
        return;
    case 0xB:
        if (Sprite_ObjectsExtra[1].step_frames != 0) return;
        area_harness::Phase(kSetGates44.v)();
        AH_CALL(Sound_PlayEffect)(0x202);
        AreaW0c_TailState = 0x14;
        return;
    case 0x14:
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
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
    unsigned char* const e = At(::bof3::Img(Sprite_ObjectsExtra_addr) + slot * area_w1b::at::kExtraStride);
    auto* const tE = rv::As<records::SpriteObject>(e);
    const std::int32_t sx = static_cast<std::int32_t>(Field_DirectionSteps[d * 2]);
    const std::int32_t sz = static_cast<std::int32_t>(Field_DirectionSteps[d * 2 + 1]);
    tE->step_x = sx >> 2;
    tE->step_z = sz >> 2;
    tE->step_y = 0;
    e[9] = 0x20;
    if (Field_MemberCount <= 1) return;
    unsigned char m = 1;
    do {
        Sprite_Current = At(::bof3::Img(ObjTrio_addr) + m * area_w1b::at::kPartyStride);
        const auto nearby = static_cast<signed char>(AH_CALL(Sprite_FindNearby)() - 0x1E);
        if (static_cast<std::int32_t>(nearby) == static_cast<std::int32_t>(slot)) {
            unsigned char* o = Cur();
            if (o[9] != 0) {
                AddLong(o + 0x34, static_cast<U>(rv::As<records::SpriteHead>(o)->step_x) * o[9]);
                o = Cur();
                AddLong(o + 0x38, static_cast<U>(rv::As<records::SpriteHead>(o)->step_z) * o[9]);
                o = Cur();
                AddLong(o + 0x3C, static_cast<U>(rv::As<records::SpriteHead>(o)->step_y) * o[9]);
            }
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, tE->step_x);
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, tE->step_z);
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, tE->step_y);
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
                AH_AT(PushPartyFn, kPushParty44.v)(7, 1);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>((B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF) | 0x20);
                AreaW0c_TailState = 1;
                return;
            }
        } else if (b == 2) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) == 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(3, 1);
                AH_CALL(Sound_PlayEffect)(0x201);
                B(::bof3::Img(area_w1b::at::kCells44.v)) = static_cast<unsigned char>(B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF);
                AreaW0c_TailState = 1;
                return;
            }
        }
        AreaW0c_TailState = 0xA;
        return;
    case 1:
        if (Sprite_ObjectsExtra[1].step_frames != 0) return;
        area_harness::Phase(kSetGates44.v)();
        [[fallthrough]];
    case 0xA: {
        bool moved = false;
        unsigned char cells = 0;
        if (a == 0) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) != 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(5, 0);
                cells = static_cast<unsigned char>((B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF2) | 2);
                moved = true;
            }
        } else if (a == 2) {
            if (AH_CALL(Flags_Test)(Bank(), 0x17) == 0) {
                AH_AT(PushPartyFn, kPushParty44.v)(1, 0);
                cells = static_cast<unsigned char>(B(::bof3::Img(area_w1b::at::kCells44.v)) & 0xF0);
                moved = true;
            }
        }
        if (moved) {
            const unsigned char state = static_cast<unsigned char>(AreaW0c_TailState);
            B(::bof3::Img(area_w1b::at::kCells44.v)) = cells;
            AH_CALL(Sound_PlayEffect)(state == 1 ? 0x20E : 0x201);
            AreaW0c_TailState = 0xB;
            return;
        }
        if (static_cast<unsigned char>(AreaW0c_TailState) == 0xB) return;
        if (static_cast<unsigned char>(AreaW0c_TailState) == 1) AH_CALL(Sound_PlayEffect)(0x202);
        AreaW0c_TailState = 0x14;
        return;
    }
    case 0xB:
        if (Sprite_ObjectsExtra[0].step_frames != 0) return;
        area_harness::Phase(kSetGates44.v)();
        AH_CALL(Sound_PlayEffect)(0x202);
        AreaW0c_TailState = 0x14;
        return;
    case 0x14:
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    default:
        return;
    }
}

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
        for (U a = ::bof3::Img(Area45_PlaceMessages_addr); a < ::bof3::Img(area_w1b::at::kA45PlaceMessagesEnd.v); a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(::bof3::Img(Area45_PlaceMessages_addr) + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = ObjTrio[0].x_whole;
        const unsigned cz = ObjTrio[0].z_whole;
        U cell = ::bof3::Img(Area45_Cells_addr);
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = ::bof3::Img(Area45_NameSets_addr); a < ::bof3::Img(area_w1b::at::kA45NameSetsEnd.v); a += 5, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(::bof3::Img(Area45_NameSets_addr) + 1 + set * 5);
        for (U row = ::bof3::Img(area_w1b::at::kTextRecords.v); row < ::bof3::Img(area_w1b::at::kTextRecordsEnd.v); row += 0x20, ++items) {
            const unsigned item = items[0];
            if (item == 0xFF) break;
            unsigned char* const out = At(row);
            if (At(::bof3::Img(area_w1b::at::kItemsHeld.v) + item)[0] == 0) {
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

// original 0x407CC0 (WorldMap_Records[2] +0; area 33's copy 0x403E00): the
// cell ahead of the leader - the high words of (leader +9) * (+0xC) + (+0x34)
// and of the same with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the
// kind +0xB by AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2,
// 0xAE 3, else Field_ScriptFlags2 bit 12 4, else 0; then Area45_PlateStates
// 0x5F7654 by +1 (read after the calls; ours aborts past its five entries).
extern "C" void __cdecl Area45_PlateRun(void) {
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
    StateEntry("Area45_PlateRun", ::bof3::Img(Area45_PlateStates_addr), 5, Cur()[1])();
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
        const unsigned place = AreaW0b_Place;
        U entry = ::bof3::Img(Area45_PlateAnims_addr);
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

// original 0x407F40 (Area45_PlateStates 2; area 33's 0x404030):
// WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0 +0x48 = 0 and +1 = 3;
// then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area45_PlateGrow(void) {
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
                            (shown == 1 && static_cast<U>(rv::As<records::EffectObject>(o)->work_18) != AreaW0b_Place);
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

// original 0x408040 (record 2 +0xC; area 33's 0x404130): Area45_HudStates
// 0x5F7668 by +1 - WorldMapHud_Start (shared), Area45_HudFrame.
extern "C" void __cdecl Area45_HudRun(void) { StateEntry("Area45_HudRun", ::bof3::Img(Area45_HudStates_addr), 2, Cur()[1])(); }

// original 0x408060 (Area45_HudStates 1; area 33's 0x404150): `call
// 0x408070; jmp 0x408140` - the frame's slide, then the region box's.
extern "C" void __cdecl Area45_HudFrame(void) {
    area_harness::Phase(kFrameStep45.v)();
    area_harness::Phase(kBoxStep45.v)();
}

// original 0x408070 (area 33's WorldMap_FrameStep's dispatch): Area45_FrameStates
// 0x5F7670 by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area45_FrameStep(void) { StateEntry("Area45_FrameStep", ::bof3::Img(Area45_FrameStates_addr), 4, Cur()[2])(); }

// original 0x408090 (Area45_FrameStates 1): the word +0x2E += 0x10; at 0x10
// and above (s16) +2 + 1; then a tail jump to Area45_FrameHold.
extern "C" void __cdecl Area45_FrameSlideIn(void) {
    rv::Current<records::EffectWork>()->work_2e = static_cast<std::uint16_t>(rv::Current<records::EffectWork>()->work_2e) + 0x10u;
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(kFrameHold45.v)();
}

// original 0x4080C0 (Area45_FrameStates 2): the mode byte 2 makes +2 = 3;
// Area45_DrawFrame(0x10, the word +0x2E). The y pushed carries a stale high
// half the drawing never reads: ours passes the word sign-extended.
extern "C" void __cdecl Area45_FrameHold(void) {
    if (AreaW0b_MapMode == 2) Cur()[2] = 3;
    AH_AT(DrawAt, kDrawFrame45.v)(0x10, S16(Cur() + 0x2E));
}

// original 0x4080F0 (Area45_FrameStates 3): the word +0x2E -= 0x10; at -0x30
// and below +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0);
// Area45_DrawFrame(0x10, y).
extern "C" void __cdecl Area45_FrameSlideOut(void) {
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
    AH_AT(DrawAt, kDrawFrame45.v)(0x10, S16(o + 0x2E));
}

// original 0x408140 (area 33's WorldMapHud_BoxStep 0x404230): Area45_BoxStates
// 0x5F7680 by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold,
// _BoxSlideOut.
extern "C" void __cdecl Area45_BoxStep(void) { StateEntry("Area45_BoxStep", ::bof3::Img(Area45_BoxStates_addr), 4, Cur()[3])(); }

// original 0x408160 (Area45_BoxStates 1; area 33's 0x404250): y +0x30 -= 10;
// at 0xC8 and below (s16) +3 + 1; when the box leaves +3 = 3;
// Area45_DrawHud(0x5C, y) - the y pushed with the object pointer's high half
// above the word, which the drawing never reads.
extern "C" void __cdecl Area45_BoxSlideIn(void) {
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
    AH_AT(DrawAt, kDrawHud45.v)(0x5C, S16(o + 0x30));
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
    AH_AT(DrawAt, kDrawHud45.v)(0x5C, S16(o + 0x30));
}

// original 0x408240 (Area45_BoxStates 3; area 33's 0x404330): y +0x30 += 10;
// at 0xF0 and above (s16) +3 = 0; unless the mode byte is set (whatever
// +0xB), Field_Request is 2 or Field_ScriptFlags bit 8, +3 = 1;
// Area45_DrawHud(0x5C, y).
extern "C" void __cdecl Area45_BoxSlideOut(void) {
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
    AH_AT(DrawAt, kDrawHud45.v)(0x5C, S16(o + 0x30));
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
    world_map_legend::Begin();   // DIV-0135: which page; the port's key names painted over
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_AT(DrawSpriteAt, kDrawSprite45.v)(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(::bof3::Img(area_w1b::at::kLeaderCellX.v)))),
                                                       static_cast<short>(Word(At(::bof3::Img(area_w1b::at::kLeaderCellZ.v)))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) AH_AT(DrawSpriteAt, kDrawSprite45.v)(x + 0x30, y, 1);
    world_map_legend::Key(AH_AT(DrawSpriteAt, kDrawSprite45.v), ::bof3::Img(Area45_Buttons_addr), 6, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000))), x + 0x38, y + 8,
                          withheld1);
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) AH_AT(DrawSpriteAt, kDrawSprite45.v)(x + 0x30, y + 0x10, 2);
    world_map_legend::Key(AH_AT(DrawSpriteAt, kDrawSprite45.v), ::bof3::Img(Area45_Buttons_addr), 8, static_cast<U>(Long(At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c))), x + 0x38, y + 0x10,
                          withheld2);
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(::bof3::Img(area_w1b::at::kLeaderCellX.v)))),
                                            static_cast<short>(Word(At(::bof3::Img(area_w1b::at::kLeaderCellZ.v))))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (AreaW0b_PartySet & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) AH_AT(DrawSpriteAt, kDrawSprite45.v)(x + 0x30, y + 0x18, 3);
    world_map_legend::Start(x + 0x30, y + 0x18, !third);
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
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->clut, 0x7B80);
    const unsigned char* const entry = At(::bof3::Img(Area45_Sprites_addr) + (index & 0xFF) * 4u);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->w, entry[0]);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(prim)->h, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x408530 (PSX 0x801F3ECC): the compass needle in the dial at (x, y)
// - the arguments' low words. Gte_PushMatrix; a local MATRIX from Camera_Angles
// by Gte_RotMatrix, its translation zeroed, Gte_SetRotMatrix and
// Gte_SetTransMatrix of it; the four corners (-10, 0, 0), (0, -4, 0), (0, 4,
// 0), (10, 0, 0) into Prim_VertexScratch (their pads untouched); a POLY_G4 at
// Gfx_PacketNext by Gpu_SetPolyG4; Gte_RotTransPers4 over the four into the
// primitive's corners with two locals for the depth and the flag (ten
// arguments, as libgte's); Gte_PrimDepths4_10B of the primitive (depths 1/4096,
// 0, 0, 0 - D41, DIV-0044 in the backend); each corner moved by (x - 0x9E,
// y - 0x76) on the x87; colours red, purple, purple, blue; Gfx_CommitPrim(1,
// 0x44); Gte_PopMatrix.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawNeedle(int x, int y) {
    g.push_matrix();
    alignas(4) unsigned char matrix[0x20];
    g.rot_matrix(reinterpret_cast<const short*>(AtWorldMap(world_map::at::Camera_AnglesAt())), reinterpret_cast<short*>(matrix));
    PutLong(matrix + 0x14, 0);
    PutLong(matrix + 0x18, 0);
    PutLong(matrix + 0x1C, 0);
    g.set_rot_matrix(reinterpret_cast<const unsigned long*>(matrix));
    g.set_trans_matrix(reinterpret_cast<const unsigned long*>(matrix));
    unsigned char* const prim = PacketNext();
    unsigned char* const scratch = AtWorldMap(world_map::at::Prim_VertexScratchAt());
    rv::As<records::SVector>(scratch)[0].x = 0xFFF6;   // (-10, 0, 0)
    rv::As<records::SVector>(scratch)[0].y = 0;
    rv::As<records::SVector>(scratch)[0].z = 0;
    rv::As<records::SVector>(scratch)[1].x = 0;        // (0, -4, 0)
    rv::As<records::SVector>(scratch)[1].y = 0xFFFC;
    rv::As<records::SVector>(scratch)[1].z = 0;
    rv::As<records::SVector>(scratch)[2].x = 0;        // (0, 4, 0)
    rv::As<records::SVector>(scratch)[2].y = 4;
    rv::As<records::SVector>(scratch)[2].z = 0;
    rv::As<records::SVector>(scratch)[3].x = 0xA;      // (10, 0, 0)
    rv::As<records::SVector>(scratch)[3].y = 0;
    rv::As<records::SVector>(scratch)[3].z = 0;
    g.set_poly_g4(prim);
    long depth, flag;
    g.rot_trans_pers4(reinterpret_cast<const short*>(scratch), reinterpret_cast<const short*>(scratch + 8),
                      reinterpret_cast<const short*>(scratch + 0x10), reinterpret_cast<const short*>(scratch + 0x18),
                      reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                      reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    g.prim_depths(prim);
    const std::int32_t dx = 0x9E - static_cast<short>(x);
    const std::int32_t dy = 0x76 - static_cast<short>(y);
    for (U corner = 0; corner < 4; ++corner) {
        unsigned char* const xy = prim + 8 + corner * 0x10;
        PutFloat(xy, X87SubInt(Float(xy), dx));
        PutFloat(xy + 4, X87SubInt(Float(xy + 4), dy));
    }
    prim[0x04] = 0xFF; prim[0x05] = 0;    prim[0x06] = 0;      // red
    prim[0x14] = 0x80; prim[0x15] = 0;    prim[0x16] = 0x80;   // purple
    prim[0x24] = 0x80; prim[0x25] = 0;    prim[0x26] = 0x80;   // purple
    prim[0x34] = 0;    prim[0x35] = 0;    prim[0x36] = 0xFF;   // blue
    g.commit_prim(1, 0x44);
    g.pop_matrix();
}

// original 0x4086D0 (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the region box (sprite 4) at (x, y), its cap (5) at
// (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low
// word of the dword 0x803584, read after the sprites; area 16's copy reads 0x803588).
extern "C" void __cdecl Area45_DrawHud(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, kDrawSprite45.v)(x, y, 4);
    AH_AT(DrawSpriteAt, kDrawSprite45.v)(x + 0x80, y, 5);
    const unsigned char* const text = At(::bof3::Img(area_w1b::at::kAreaText.v) + (static_cast<U>(Long(At(::bof3::Img(area_w1b::at::kAreaTextOffset45.v)))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// original 0x408730 (record 2 +8): Area45_Record8States 0x5F7700 by +1 -
// 0x4253C0 and 0x40C490 (the world-map copies' shared states, not this
// group's) around Area45_Record8Place.
extern "C" void __cdecl Area45_Record8Run(void) { StateEntry("Area45_Record8Run", ::bof3::Img(Area45_Record8States_addr), 3, Cur()[1])(); }

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
    const auto dir = [](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(::bof3::Img(Area45_Directions_addr) + half + o[8] * 4u))); };
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
    AH_CALL(Sprite_SetAnimation)(At(::bof3::Img(Area45_Record8Anims_addr) + Cur()[8] * 2u)[0]);
    o = Cur();
    tO = rv::As<records::EffectObject>(o);
    o[0x2A] = At(::bof3::Img(Area45_Record8Anims_addr) + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x4088B0 (record 2 +4): Area45_Record4States 0x5F7724 by +1 -
// Area45_Record4MarkCell, then Area45_Record4Tick 0x408990 (shared by ten
// world maps).
extern "C" void __cdecl Area45_Record4Run(void) { StateEntry("Area45_Record4Run", ::bof3::Img(Area45_Record4States_addr), 2, Cur()[1])(); }

// original 0x4088D0 (Area45_Record4States 0; area 33's copy 0x404820): with
// the byte 0x903A79 9 or Field_StatusBits bit 0, a tail jump to
// Effect_Release. Else WorldMap_RecordIndex (its answer not read),
// Sprite_SetAnimationBank(0x205); +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F =
// 0; the map byte of the cell record Area45_Cells[+0xB] (x, z; unchecked) set
// to 0xA0 - AreaMap_Bytes + AreaMap_Header[0] * z + x; Sprite_SetAnimation(0)
// and +1 = 1.
extern "C" void __cdecl Area45_Record4MarkCell(void) {
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
    const U record = ::bof3::Img(Area45_Cells_addr) + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[z * width + x] = 0xA0;
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
    std::int32_t dx = S16(At(::bof3::Img(area_w1b::at::kLeaderCellX.v))) - S16(o + 0x36);
    if (dx < 0) dx = -dx;
    if (dx > 0x19) {
        std::int32_t dz = S16(At(::bof3::Img(area_w1b::at::kLeaderCellZ.v))) - S16(o + 0x3A);
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
            const U m = At(::bof3::Img(Area45_DriftUV_addr) + 0x8 + static_cast<U>(e))[0];
            const U ubase = At(::bof3::Img(Area45_DriftUV_addr) + static_cast<U>(e))[0];
            const U vbase = At(::bof3::Img(Area45_DriftUV_addr) + 0x4 + static_cast<U>(e))[0];
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

void Area45_Inject() {
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
    BOF3_INJECT(WorldMap_DrawNeedle);
    BOF3_INJECT(Area45_DrawHud);
    BOF3_INJECT(Area45_Record8Run);
    BOF3_INJECT(Area45_Record8Place);
    BOF3_INJECT(Area45_Record4Run);
    BOF3_INJECT(Area45_Record4MarkCell);
    BOF3_INJECT(Area45_Record4Tick);
    BOF3_INJECT(Area45_DrawDrift);
}

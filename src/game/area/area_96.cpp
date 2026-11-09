// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_96.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// Areas 95 and 96's handler 4 (0x4135B0, 0x413860, byte for byte the same):
// the running object at z word 0x3A and x word 0x40 turns to 3, at x 0x42 to
// 7, and runs Area95_TurnAndMove (the originals' tail jmp); anywhere else
// the script position + 2.
void TurnAtCell() {
    unsigned char* const cur = Sprite_Current;
    if (rv::As<records::SpriteObject>(cur)->z_whole == 0x3A) {
        const auto d = static_cast<std::uint16_t>(rv::As<records::SpriteObject>(cur)->x_whole - 0x40);
        if (d == 0 || d == 2) {
            cur[8] = d == 0 ? 3 : 7;
            AH_CALL(Area95_TurnAndMove)();
            return;
        }
    }
    ScriptStep(2);
}

}  // namespace

// original 0x413860 (area 96 +0x3C[4]; PSX 0x801F34AC): TurnAtCell, area
// 96's copy.
extern "C" void __cdecl Area96_TurnAtCell(void) { TurnAtCell(); }

// original 0x4138A0 (areas 95 and 96 +0x3C[5]; PSX 0x801F35A4 / 0x801F3534):
// the map cell (0x41, 0x39) set to 0x10 when it is 0, else to 0.
extern "C" void __cdecl Area96_ToggleCell(void) {
    if (AH_CALL(AreaMap_ByteAt)(0x41, 0x39) == 0) {
        SetByte(0x41, 0x39, 0x10);
        return;
    }
    SetByte(0x41, 0x39, 0);
}

// original 0x4138D0 (areas 95 and 96 +0x3C[6]; PSX 0x801F35F0 / 0x801F3580):
// the running object's direction 3 when it is 7, else 7.
extern "C" void __cdecl Area96_FaceBack(void) {
    unsigned char* const cur = Sprite_Current;
    cur[8] = cur[8] == 7 ? 3 : 7;
}

// original 0x4138F0 (areas 95 and 96 +0x3C[7]; PSX 0x801F3618 / 0x801F35A8):
// the ground at the leader's (x, z), sign-extended; MapView_Elevation already
// there: 0x20 lower, else set to it; MapView_Redraw + 2 either way.
extern "C" void __cdecl Area96_SnapElevation(void) {
    const long ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34)), Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38))));
    const long now = MapView_Elevation;
    MapView_Elevation = now == ground ? now - 0x20 : ground;
    MapView_Redraw = static_cast<unsigned char>(MapView_Redraw + 2);
}

// original 0x413940 (Field_ModeTailKinds[13]): by the s8 state, 0 -
// ScriptFlags_Set40 and state 1; 1 - once counter 3 is 2, ScriptFlags_Clear40,
// counter 3 0 and the tail disarmed.
extern "C" void __cdecl Area97_TailWaitCounter3(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    if (state == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    }
    if (state != 1 || B(::bof3::Img(at::kCounter3.v)) != 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(at::kCounter3.v)) = 0;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x413980 (Area_StepHook's case for area 97): z at most 0x3A0000
// (signed), x's high word 0x30..0x32 (a 16-bit compare), the leader's pose 0,
// 1 or 2, and key item 5 not held: tail kind 13 armed (state 0, counter 3 =
// 1), al 1. Else al 0.
extern "C" unsigned char __cdecl Area97_StepHook(long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (z > 0x3A0000) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x30) >= 3) return 0;
    if (pose != 0 && pose != 1 && pose != 2) return 0;
    if (AH_CALL(KeyItem_Has)(5) != 0) return 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(at::kCounter3.v)) = 1;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xD;
    return 1;
}

void Area96_Inject() {
    BOF3_INJECT(Area96_TurnAtCell);
    BOF3_INJECT(Area96_ToggleCell);
    BOF3_INJECT(Area96_FaceBack);
    BOF3_INJECT(Area96_SnapElevation);
    BOF3_INJECT(Area97_TailWaitCounter3);
    BOF3_INJECT(Area97_StepHook);
}

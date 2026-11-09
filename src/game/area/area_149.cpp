// Moved here by tools/move_funcs.py from area_w3g.cpp (docs/source-layout-round.md).
#include "game/area/area_149.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3g_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3g::at;

}  // namespace

namespace {

using U = std::uint32_t;

using area_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

unsigned char* Bank904000() { return At(::bof3::Img(Cond_Flags_addr) + 0x70); }

unsigned char* EffectAt(U slot) { return At(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride); }

unsigned char& B(U address) { return move_script::At(address)[0]; }

void Disarm() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
}

// The tails' u16 frame counter less one: true when it reached 0.
bool TimerDone() {
    const auto t = static_cast<std::uint16_t>(Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
    SetWord(At(::bof3::Img(at::kTailTimer.v)), t);
    return t == 0;
}

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// The dust states' shared tail: Area149_SpawnDust(dx), then the running
// script's word +0xA - 2 (MoveScript_Object's: the op runs again).
void DustThenAgain(int dx) {
    AH_CALL(Area149_SpawnDust)(static_cast<signed char>(dx));
    unsigned char* const script = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(script)->position, rv::As<records::ScriptContext>(script)->position + 0xFFFEu);
}

// Extra record 0's x (s32 +0x34) below the kind-2 x + 0x20000 (signed): its
// +0x84 = 3.
void MarkIfBehind() {
    if (Sprite_ObjectsExtra[0].x < static_cast<std::int32_t>(static_cast<U>(Field_Kind2X) + 0x20000u))
        Sprite_ObjectsExtra[0].speed = 3;
}

// The map moved 0x14 cells in x (`sign` +1 or -1): Sprite_Kind2 +0x34 and
// Field_Kind2X by 0x140000, MapView_Origin's word by 0x14, MapView_FocusX to
// `focus`; the kind-2 sprite's height +0x3E from AreaMap_Elevation there,
// MapView_SetElevation(it), the view shift 0x56FCA0; with `effects` every
// live (bit 0) dust record (kind 0x37) of the twenty moved and its +0x3E
// again; extra record 0's x moved and its +0x3E again.
void ShiftMap(int sign, U focus, bool effects) {
    const U step = sign > 0 ? 0x140000u : 0xFFEC0000u;
    const U kind2x = static_cast<U>(Field_Kind2X);
    MapView_Origin[0] = static_cast<std::uint16_t>(MapView_Origin[0]) + (sign > 0 ? 0x14u : 0xFFECu);
    Sprite_Kind2.x = static_cast<std::int32_t>(static_cast<std::uint32_t>(Sprite_Kind2.x) + step);
    const std::int32_t x = static_cast<std::int32_t>(kind2x + step);
    const std::int32_t z = Field_Kind2Z;
    MapView_FocusX = static_cast<std::int32_t>(focus);
    Field_Kind2X = x;
    const long h = AH_CALL(AreaMap_Elevation)(x, z);
    Sprite_Kind2.height_whole = static_cast<U>(h);
    AH_CALL(MapView_SetElevation)(static_cast<int>(h));
    AH_AT(void (__cdecl*)(), area_w3g::kViewShift.v)();
    if (effects) {
        for (U k = 0; k < at::kEffectCount; ++k) {
            unsigned char* const e = EffectAt(k);
            auto* const tE = rv::As<records::EffectObject>(e);
            if ((e[0] & 1) == 0 || e[5] != 0x37) continue;
            tE->x = static_cast<std::int32_t>(static_cast<U>(tE->x) + step);
            const long eh = AH_CALL(AreaMap_Elevation)(tE->x, tE->z);
            rv::Store<unsigned>(rv::As<records::EffectObject>(e)->height_whole, static_cast<U>(eh));
        }
    }
    const std::int32_t ex = static_cast<std::int32_t>(static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].x) + step);
    Sprite_ObjectsExtra[0].x = ex;
    const long xh = AH_CALL(AreaMap_Elevation)(ex, Sprite_ObjectsExtra[0].z);
    Sprite_ObjectsExtra[0].height_whole = static_cast<U>(xh);
}

}  // namespace

// original 0x423160 (handler 1; PSX 0x801F2EFC): Area149_DustStates 0x6361FC
// by Sprite_Current[4] (4; ours aborts past).
extern "C" void __cdecl Area149_DustRun(void) {
    StateEntry("Area149", "DustRun", ::bof3::Img(Area149_DustStates_addr), at::kA149DustStateCount, Cur()[4])();
}

// original 0x423180 (dust state 0): Camera_Distance (s16) below 0x880: extra
// record 0's +0x84 = 1, counter 0 = 0xA, Sound_PlayEffect(1), +4 + 1. Then
// the dust at -4 and the op again.
extern "C" void __cdecl Area149_DustNear880(void) {
    if (Camera_Distance < 0x880) {
        Sprite_ObjectsExtra[0].speed = 1;
        B(::bof3::Img(at::kCounter0.v)) = 0xA;
        AH_CALL(Sound_PlayEffect)(1);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    DustThenAgain(-4);
}

// original 0x4231D0 (dust state 1): below 0x680: counter 0 = 0xF,
// Sound_PlayEffect(0), +4 + 1. Then extra record 0 behind the kind-2 x + 2
// cells: its +0x84 = 3; the dust at -3 and the op again.
extern "C" void __cdecl Area149_DustNear680(void) {
    if (Camera_Distance < 0x680) {
        B(::bof3::Img(at::kCounter0.v)) = 0xF;
        AH_CALL(Sound_PlayEffect)(0);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    MarkIfBehind();
    DustThenAgain(-3);
}

// original 0x423230 (dust state 2): Camera_Distance 0: counter 0 = 0x14,
// Sound_PlayEffect(1), +4 + 1. Then as state 1, the dust at -2.
extern "C" void __cdecl Area149_DustAtZero(void) {
    if (Camera_Distance == 0) {
        B(::bof3::Img(at::kCounter0.v)) = 0x14;
        AH_CALL(Sound_PlayEffect)(1);
        Cur()[4] = static_cast<unsigned char>(Cur()[4] + 1);
    }
    MarkIfBehind();
    DustThenAgain(-2);
}

// original 0x423290 (dust state 3): the dust at -1 and the op again.
extern "C" void __cdecl Area149_DustHold(void) { DustThenAgain(-1); }

// original 0x4232B0 (dx, a signed byte): nothing on frames with Frame_Counter
// & 3. Else twice (z one cell ahead, then one behind): an Effect_FindFree slot
// (none: the next) of kind 0x37 at (Field_Kind2X + dx << 16, Field_Kind2Z +-
// 0x10000), the word +0x2C 0x2C0 (twice), +0x3E the elevation there, +9 1, +6
// and +7 0, +0x29 6.
extern "C" void __cdecl Area149_SpawnDust(signed char dx) {
    if ((Frame_Counter & 3) != 0) return;
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot == 0xFF) continue;
        unsigned char* const e = EffectAt(slot);
        auto* const tE = rv::As<records::EffectObject>(e);
        const U x = (static_cast<U>(static_cast<std::int32_t>(dx)) << 16) + static_cast<U>(Field_Kind2X);
        const U z = (k != 0 ? 0xFFFF0000u : 0x10000u) + static_cast<U>(Field_Kind2Z);
        e[0] = 1;
        e[5] = 0x37;
        rv::Store<unsigned>(rv::As<records::EffectObject>(e)->column, 0x2C0);
        tE->x = static_cast<std::int32_t>(x);
        tE->z = static_cast<std::int32_t>(z);
        const long h = AH_CALL(AreaMap_Elevation)(tE->x, static_cast<long>(z));
        rv::Store<unsigned>(rv::As<records::EffectObject>(e)->height_whole, static_cast<U>(h));
        rv::Store<unsigned>(rv::As<records::EffectObject>(e)->column, 0x2C0);
        e[9] = 1;
        e[6] = 0;
        e[7] = 0;
        e[0x29] = 6;
    }
}

// original 0x423380 (called by Scena15_Runs' step 0x33, 0x56ABC4): with
// MapView_FocusX at 0x63FF the map moved 0x14 cells back in x (focus 0x77FF),
// the dust records with it.
extern "C" void __cdecl Area149_ViewShiftBack(void) {
    if (static_cast<std::uint32_t>(MapView_FocusX) != 0x63FF) return;
    ShiftMap(-1, 0x77FF, true);
}

// original 0x423450 (Field_ModeTailKinds[33]): by the s8 0x9039F4 through the
// byte table 0x423610 and the case table 0x4235FC (states 0..0xC):
//   0: with MapView_FocusX at 0x77FF the map moved 0x14 cells on (focus
//      0x63FF; no dust records); the timer - 1, at 0 state 2.
//   2: the script byte 0x90384B = 0x20, the tail disarmed,
//      Field_ChangeArea(0xA7, 0x280000, 0x630000, 0x81).
//   0xA: with the focus at 0x63FF the map moved back (focus 0x77FF); the
//      timer - 1, at 0 state 0xC.
//   0xC: as 2 to area 0x94 (0xA0000, 0x190000, 0x81).
extern "C" void __cdecl Area149_Tail33(void) {
    switch (AreaW0c_TailState) {
    case 0:
        if (static_cast<std::uint32_t>(MapView_FocusX) == 0x77FF) ShiftMap(1, 0x63FF, false);
        if (TimerDone()) AreaW0c_TailState = 2;
        return;
    case 2:
        B(::bof3::Img(at::kScriptVar.v)) = 0x20;
        Disarm();
        AH_CALL(Field_ChangeArea)(0xA7, 0x280000, 0x630000, 0x81);
        return;
    case 0xA:
        if (static_cast<std::uint32_t>(MapView_FocusX) == 0x63FF) ShiftMap(-1, 0x77FF, false);
        if (TimerDone()) AreaW0c_TailState = 0xC;
        return;
    case 0xC:
        B(::bof3::Img(at::kScriptVar.v)) = 0x20;
        Disarm();
        AH_CALL(Field_ChangeArea)(0x94, 0xA0000, 0x190000, 0x81);
        return;
    default:
        return;
    }
}

// original 0x423620 (area 149's init; PSX 0x801F36CC): come from area 0x94:
// extra record 0's x high word + 0x16, +0x83 = 1, the word +0x8A = 0, +0x3E
// its elevation, Cond_ByteFE = 1; with flag 0x11 of the bank 0x904000 tail
// kind 33 at state 0 and the timer 0x5A. Come from area 0xA7: +0x8A = 0, +0x83
// and Cond_ByteFE 2, tail kind 33 at state 0xA, the timer 0x5A.
extern "C" void __cdecl Area149_Init(void) {
    unsigned char* const x0 = At(::bof3::Img(Sprite_ObjectsExtra_addr));
    auto* const tX0 = rv::As<records::SpriteObject>(x0);
    if (AreaW2c_LastArea == 0x94) {
        rv::Store<unsigned>(rv::As<records::SpriteObject>(x0)->x_whole, rv::As<records::SpriteObject>(x0)->x_whole + 0x16u);
        const std::int32_t z = tX0->z, x = tX0->x;
        x0[0x83] = 1;
        tX0->script_position = 0;
        const long h = AH_CALL(AreaMap_Elevation)(x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(x0)->height_whole, static_cast<U>(h));
        Cond_ByteFE = 1;
        if (AH_CALL(Flags_Test)(Bank904000(), 0x11) != 0) {
            AreaW0a_TailKind = 0x21;
            AreaW0c_TailState = 0;
            SetWord(At(::bof3::Img(at::kTailTimer.v)), 0x5A);
        }
    }
    if (AreaW2c_LastArea == 0xA7) {
        tX0->script_position = 0;
        x0[0x83] = 2;
        Cond_ByteFE = 2;
        AreaW0a_TailKind = 0x21;
        AreaW0c_TailState = 0xA;
        SetWord(At(::bof3::Img(at::kTailTimer.v)), 0x5A);
    }
}

void Area149_Inject() {
    BOF3_INJECT(Area149_DustRun);
    BOF3_INJECT(Area149_DustNear880);
    BOF3_INJECT(Area149_DustNear680);
    BOF3_INJECT(Area149_DustAtZero);
    BOF3_INJECT(Area149_DustHold);
    BOF3_INJECT(Area149_SpawnDust);
    BOF3_INJECT(Area149_ViewShiftBack);
    BOF3_INJECT(Area149_Tail33);
    BOF3_INJECT(Area149_Init);
}

// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_99.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
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

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The callee nobody owns (area_w2d_callees.h): story-flag toggle.
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// Field_ActiveMember's word +0x8A less 2.
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

// Field_State's word +0x12E less 2.
void LeaderStep() { AddWord(Field_State + 0x12E, -2); }

}  // namespace

// original 0x413CA0 (areas 39 +0x3C[2], 41 +0x34[11] = +0x3C[1], 99
// +0x3C[1]; PSX 0x801F2C24 in area 99): Camera_Distance 0, MapView_Redraw 2.
extern "C" void __cdecl Area98_CameraDistance0(void) {
    Camera_Distance = 0;
    MapView_Redraw = 2;
}

// original 0x413CC0 (area 99 +0x3C[2]; PSX 0x801F2C40): the running object's
// direction, if even, turned one step back (& 7); Sound_PlayEffect(its word
// +0x2C + 0x100); Sprite_EnsureAnimation(0x42 + (direction - 1) / 2, a
// signed halving); Field_State +0x124 bit 0x40; Effect_FindFree to its +0xB
// and, a slot (read back from +0xB), kind 0x1B. Sprite_Current read again
// after each call.
extern "C" void __cdecl Area99_TurnSoundEffect1B(void) {
    unsigned char* cur = Sprite_Current;
    if ((cur[8] & 1) == 0) {
        cur[8] = static_cast<unsigned char>((cur[8] - 1) & 7);
        cur = Sprite_Current;
    }
    AH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(cur + 0x2C) + 0x100));
    const int half = (static_cast<int>(Sprite_Current[8]) - 1) / 2;
    AH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(half + 0x42));
    Field_State[0x124] = static_cast<unsigned char>(Field_State[0x124] | 0x40);
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char* const obj = Sprite_Current;
    if (obj[0xB] == 0xFF) return;
    EffectAt(obj[0xB])[0] = 1;
    EffectAt(obj[0xB])[5] = 0x1B;
}

// original 0x413D50 (area 99 +0x3C[3]; PSX 0x801F2D68): Area99_DriftStates
// by Sprite_Current[4].
extern "C" void __cdecl Area99_RunDrift(void) {
    StateEntry("Area99_RunDrift", ::bof3::Img(Area99_DriftStates_addr), at::kArea99StateCount, Sprite_Current[4])();
}

// original 0x413D70 (Area99_DriftStates[0]): +0xA = 0x48, state 1 (the object
// read again); the active member's word +0x8A - 2.
extern "C" void __cdecl Area99_DriftStart(void) {
    Sprite_Current[0xA] = 0x48;
    Sprite_Current[4] = 1;
    MemberStep();
}

// original 0x413DA0 (Area99_DriftStates[1]): while +0xA: z (+0x38) moved by
// the negated signed step Area99_DriftSteps[+0xA & 0xF] << 11, +0xA - 1 (the
// object read again), the active member's word +0x8A - 2; at 0, state 0.
extern "C" void __cdecl Area99_DriftStep(void) {
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area99_DriftSteps_addr) + (cur[0xA] & 0xF))));
    AddLong(cur + 0x38, (0u - static_cast<U>(step)) << 11);
    unsigned char* const obj = Sprite_Current;
    obj[0xA] = static_cast<unsigned char>(obj[0xA] - 1);
    MemberStep();
}

// original 0x413DF0 (area 99 +0x3C[4]; PSX 0x801F2E58): Area99_LeapStates by
// Sprite_Current[4].
extern "C" void __cdecl Area99_RunLeap(void) {
    StateEntry("Area99_RunLeap", ::bof3::Img(Area99_LeapStates_addr), at::kArea99StateCount, Sprite_Current[4])();
}

// original 0x413E10 (Area99_LeapStates[0]): speed Field_MoveSpeeds[3]; +9 =
// 0, +0xB = 3, frames +0xA = 16 / speed, +0x14 = 0x4200, +0xC = 0x20000 /
// (frames * 4), +0x10 = -0x18000 / (frames * 4) (idivs: a speed of 0, or
// above 16, faults; ours aborts); then +0xA = 0xE, state 1; Field_State's
// word +0x12E - 2. Sprite_Current read again for every store.
extern "C" void __cdecl Area99_LeapStart(void) {
    const unsigned speed = B(::bof3::Img(Field_MoveSpeeds_addr) + 0x3);
    Sprite_Current[9] = 0;
    Sprite_Current[0xB] = 3;
    if (speed == 0) bof3::Fatal("Area99_LeapStart: Field_MoveSpeeds[3] is 0, the original's idiv faults");
    Sprite_Current[0xA] = static_cast<unsigned char>(16 / static_cast<int>(speed));
    rv::Current<records::SpriteObject>()->step_y = 0x4200;
    unsigned char* cur = Sprite_Current;
    auto* tCur = rv::As<records::SpriteObject>(cur);
    std::int32_t d = static_cast<std::int32_t>(cur[0xA]) * 4;
    if (d == 0) bof3::Fatal("Area99_LeapStart: 16 / Field_MoveSpeeds[3] is 0, the original's idiv faults");
    tCur->step_x = 0x20000 / d;
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    d = static_cast<std::int32_t>(cur[0xA]) * 4;
    if (d == 0) bof3::Fatal("Area99_LeapStart: 16 / Field_MoveSpeeds[3] is 0, the original's idiv faults");
    tCur->step_z = -0x18000 / d;
    Sprite_Current[0xA] = 0xE;
    Sprite_Current[4] = 1;
    LeaderStep();
}

// original 0x413EC0 (Area99_LeapStates[1]): x += +0xC, z += +0x10, the word
// +0x3E += +0x14 >> 8 (arithmetic), +0x14 -= 0x700, +0xA - 1; rising before
// and falling now: Sprite_EnsureAnimation(0x51); then +0xA below 1: state 0,
// else Field_State's word +0x12E - 2. Sprite_Current read again for each.
extern "C" void __cdecl Area99_LeapStep(void) {
    unsigned char* cur = Sprite_Current;
    auto* tCur = rv::As<records::SpriteObject>(cur);
    AddLong(cur + 0x34, static_cast<U>(tCur->step_x));
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    AddLong(cur + 0x38, static_cast<U>(tCur->step_z));
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    AddWord(cur + 0x3E, static_cast<int>(static_cast<std::uint16_t>(static_cast<U>(tCur->step_y >> 8))));
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    const std::int32_t rise = tCur->step_y;
    tCur->step_y = static_cast<std::int32_t>(static_cast<U>(rise) - 0x700u);
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    if (rise > 0 && rv::Current<records::SpriteObject>()->step_y < 0) AH_CALL(Sprite_EnsureAnimation)(0x51);
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    if (cur[0xA] < 1) {
        cur[4] = 0;
        return;
    }
    LeaderStep();
}

// original 0x413F50 (area 99 +0x3C[5]; PSX 0x801F30A8): the chapter row's
// flag 0x1F toggled (0x57C160).
extern "C" void __cdecl Area99_ToggleRowFlag1F(void) { FlagsToggle(FlagRow(), 0x1F); }

// original 0x413F70 (area 99 +0x40; PSX 0x801F30D0): Cond_ByteFD 1: cell
// (0x23, 0x4F) = 0x51 when Cond row 3's flag 0x1C is set and 0x1D clear, else
// 0x50.
extern "C" void __cdecl Area99_InitCell(void) {
    if (Cond_ByteFD != 1) return;
    unsigned char* const row = Mem(::bof3::Img(Cond_Flags_addr) + 0x18);
    if (AH_CALL(Flags_Test)(row, 0x1C) != 0 && AH_CALL(Flags_Test)(row, 0x1D) == 0) {
        SetByte(0x23, 0x4F, 0x51);
        return;
    }
    SetByte(0x23, 0x4F, 0x50);
}

void Area99_Inject() {
    BOF3_INJECT(Area98_CameraDistance0);
    BOF3_INJECT(Area99_TurnSoundEffect1B);
    BOF3_INJECT(Area99_RunDrift);
    BOF3_INJECT(Area99_DriftStart);
    BOF3_INJECT(Area99_DriftStep);
    BOF3_INJECT(Area99_RunLeap);
    BOF3_INJECT(Area99_LeapStart);
    BOF3_INJECT(Area99_LeapStep);
    BOF3_INJECT(Area99_ToggleRowFlag1F);
    BOF3_INJECT(Area99_InitCell);
}

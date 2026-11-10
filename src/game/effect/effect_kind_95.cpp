// Moved here by tools/move_funcs.py from effect_4d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_95.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4d.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// Kind 0x95's two waits: a message open or opening (Field_Request 2 or 1),
// Game_Mode 4 or 3, the given ObjTrio byte 2, or the menu button pressed - in
// the original's order (each read once, as it reads them).
bool Kind95Held(U trio) {
    const unsigned char request = Field_Request;
    if (request == 2) return true;
    const unsigned short mode = Game_Mode;
    if (mode == 4) return true;
    if (At(trio)[0] == 2) return true;
    if (request == 1) return true;
    if (mode == 3) return true;
    return (Input_Pressed & Field_MenuButton) != 0;
}

}  // namespace

// original 0x48CDD0 (Effect_KindHandlers[0x95], hidden in 0x48CD40):
// jmp [EffectKind95_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind95_Run(void) {
    Dispatch("EffectKind95_Run", AddressOf(EffectKind95_States), EffectKind95_States_count);
}

// original 0x48CDF0 (state 0): the clock word 0x67629A 0; +1 up.
extern "C" void __cdecl EffectKind95_Start(void) {
    unsigned char* const s = S();
    Effect4d_Kind95Clock = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x48CE10 (state 1): unless held (Field_Request 2, Game_Mode 4, the
// byte 0x80310F 2, Field_Request 1, Game_Mode 3, the menu button pressed), the
// clock's frame count (the high byte's bits 0..6) up 1; at 30 (40 when bit 7 is
// set) it is 0 and the ticks (the low byte) up 1. At 15 ticks: +1 up.
extern "C" void __cdecl EffectKind95_Clock(void) {
    if (Kind95Held(::bof3::Img(ObjTrio_addr) + 0x3cf)) return;
    const unsigned w = Effect4d_Kind95Clock;
    auto ticks = static_cast<unsigned char>(w);
    const unsigned mode = (w >> 15) & 1u;
    auto frames = static_cast<unsigned char>(((w >> 8) & 0x7Fu) + 1u);
    if (frames >= (mode == 0 ? 0x1Eu : 0x28u)) {
        frames = 0;
        ticks = static_cast<unsigned char>(ticks + 1);
    }
    const auto high = static_cast<unsigned char>((mode << 7) | frames);
    Effect4d_Kind95Clock = (static_cast<unsigned>(high) << 8) | ticks;
    if (ticks >= 0xF) Step();
}

// original 0x48CEA0 (state 2): unless held (as the clock, but the byte
// 0x802E77): ScriptFlags_Set40; the chapter's step 0x8034E5 = 5 and run
// MoveScript_Var7 = 7; a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind95_Finish(void) {
    if (Kind95Held(::bof3::Img(ObjTrio_addr) + 0x137)) return;
    SH_CALL(ScriptFlags_Set40)();
    MoveScript_PartyRecords[0x65] = 5;
    MoveScript_Var7 = 7;
    SH_CALL(Effect_Release)();
}

// original 0x48CF00 (Effect_KindHandlers[0x96], hidden in 0x48CD40):
// jmp [EffectKind96_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind96_Run(void) {
    Dispatch("EffectKind96_Run", AddressOf(EffectKind96_States), EffectKind96_States_count);
}

// original 0x48CF20 (state 0): +9 0, +1 = 1; the level 0x676298 = 0xF and the
// index 0x676299 0.
extern "C" void __cdecl EffectKind96_Start(void) {
    S()[9] = 0;
    S()[1] = 1;
    Effect4d_Kind96Shade = 0xF;
    Effect4d_Kind96Index = 0;
}

// original 0x48CF50 (state 1): +9 up; when it was 0x10 or more, the level is
// EffectKind96_Shades[index], +9 0, and the index up 1 (0 after 7). Then a draw
// mode (page 2 at (0x140, 0x140), blend 2, dtd 1) committed at slot 6 (0xC); a
// POLY_G4 at the cursor (read after the commit) of (0, 0), (320, 0), (0, 320),
// (320, 320) (floats), every corner coloured (level, 0, level); semi-
// transparent through the cursor read again; committed at slot 2 (0x44). An
// index past the table's eight aborts (docs/effect_4d.md section 6). Area
// 118's purple pulse (the owner's radiationRoom.txt); widened under the wide
// picture (DIV-0041, 2026-10-15).
extern "C" void __cdecl EffectKind96_Pulse(void) {
    unsigned char* const s = S();
    const unsigned char count = s[9];
    s[9] = static_cast<unsigned char>(count + 1);
    if (count >= 0x10) {
        unsigned char* const r = S();
        const unsigned index = Effect4d_Kind96Index;
        if (index >= EffectKind96_Shades_count)
            bof3::Fatal("EffectKind96_Pulse: the index 0x676299 is %u, past EffectKind96_Shades' %u - the original "
                        "reads the state table after it as a level (docs/effect_4d.md section 6)",
                        index, (unsigned)EffectKind96_Shades_count);
        Effect4d_Kind96Shade = EffectKind96_Shades[index];
        r[9] = 0;
        const unsigned char i = Effect4d_Kind96Index;
        Effect4d_Kind96Index = static_cast<unsigned char>(i + 1);
        if (i >= 7) Effect4d_Kind96Index = 0;
    }
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(2, 2, 0x140, 0x140) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(prim);
    constexpr U k320 = 0x43A00000u;   // 320.0f
    // DIV-0041: the left corners at Widescreen_FillX(), the right at 320 + the
    // columns, as EffectKind18Sub36_Pulse; (0, 320) narrow or unarmed. The y
    // corners stay Capcom's 0 and 320.
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillRight(320.0f));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 8, left);
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x18, right);
    SetUL(prim + 0x1C, 0);
    SetUL(prim + 0x28, left);
    SetUL(prim + 0x2C, k320);
    SetUL(prim + 0x38, right);
    SetUL(prim + 0x3C, k320);
    for (const U corner : {0x4u, 0x14u, 0x24u, 0x34u}) {
        const unsigned char level = Effect4d_Kind96Shade;
        prim[corner] = level;
        prim[corner + 1] = 0;
        prim[corner + 2] = level;
    }
    SH_CALL(Gpu_SetSemiTrans)(Gfx_PacketNext, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
}

void EffectKind95_Inject() {
    BOF3_INJECT(EffectKind95_Run);
    BOF3_INJECT(EffectKind95_Start);
    BOF3_INJECT(EffectKind95_Clock);
    BOF3_INJECT(EffectKind95_Finish);
    BOF3_INJECT(EffectKind96_Run);
    BOF3_INJECT(EffectKind96_Start);
    BOF3_INJECT(EffectKind96_Pulse);
}

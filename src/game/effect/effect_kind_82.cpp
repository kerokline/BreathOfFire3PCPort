// Moved here by tools/move_funcs.py from effect_3d.cpp, effect_4a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_82.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_82_callees.h"
#include "game/effect_3d.h"
#include "game/effect_3d_callees.h"
#include "game/effect_4a.h"
#include "game/effect_4a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_82::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// Field_State the leader; true when Sprite_Objects record 0's x is the
// leader's or past it (signed).
bool ObjectReached() {
    const auto object_x = static_cast<std::int32_t>(static_cast<std::uint32_t>(Sprite_Objects[0].x));
    const auto leader_x = static_cast<std::int32_t>(static_cast<std::uint32_t>(ObjTrio[0].x));
    Field_State = rv::Bytes(ObjTrio);
    return object_x >= leader_x;
}

// Record 0 a cell on (x + 0x4000 four times, Sprite_Current pointed at it for
// the adds as the original does), +1 `next`.
void Push(unsigned next) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = rv::Bytes(Sprite_Objects);
    for (int n = 4; n != 0; --n) SetUL(Sprite_Current + 0x34, UL(Sprite_Current + 0x34) + 0x4000u);
    saved[1] = static_cast<unsigned char>(next);
    Sprite_Current = saved;
}

// +1 0xD and +9 0 when record 0 has reached the leader; else +1 `next`.
void Check(unsigned next) {
    unsigned char* const s = S();
    if (ObjectReached()) {
        s[1] = 0xD;
        S()[9] = 0;
    } else {
        s[1] = static_cast<unsigned char>(next);
    }
}

std::int32_t SL(U a) { return Long(At(a)); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table, a null or data.
void DispatchEffect4a(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4a.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(P(table + 4 * state))))();
}

// Field_State the leader; true when the dword at `x` (a record's +0x34) is the
// leader's x or past it (signed).
bool Reached(U x) {
    const std::int32_t object_x = SL(x);
    const std::int32_t leader_x = ObjTrio[0].x;
    Field_State = rv::Bytes(ObjTrio);
    return object_x >= leader_x;
}

// +1 0xD and +9 0 when the record at `x` has reached the leader; else +1 `next`.
void CheckEffect4a(U x, unsigned next) {
    unsigned char* const s = S();
    if (Reached(x)) {
        s[1] = 0xD;
        S()[9] = 0;
    } else {
        s[1] = static_cast<unsigned char>(next);
    }
}

// Sprite_Current pointed at `object` for `count` adds of `step` to its dword
// `offset`, then +1 `next` (and +9 `wait` when given) on the effect record and
// Sprite_Current put back, in the original's order.
void Nudge(unsigned char* object, unsigned offset, int count, U step, unsigned next, int wait) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = object;
    for (int n = count; n != 0; --n) SetUL(Sprite_Current + offset, UL(Sprite_Current + offset) + step);
    saved[1] = static_cast<unsigned char>(next);
    Sprite_Current = saved;
    if (wait >= 0) saved[9] = static_cast<unsigned char>(wait);
}

// The pushes have ended (0x903849 0xFF) or the presses have (0x90384A 0x80):
// +1 0x17, +2 0; else 0x903849 0 and +1, +2 0 (back to the start).
void Again() {
    if (B(::bof3::Img(effect_4a::at::kCounterB.v)) == 0xFF || B(::bof3::Img(effect_4a::at::kCounterC.v)) == 0x80) {
        S()[1] = 0x17;
        S()[2] = 0;
        return;
    }
    unsigned char* const s = S();
    B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0;
    s[1] = 0;
    S()[2] = 0;
}

// Field_State the third member's object; its x at 0x34 cells or past it
// (signed): true, with 0x903849 0xFF, 0x90384A / B 0 and the step 0x14.
bool PastLine() {
    const std::int32_t x = ObjTrio[2].x;
    Field_State = P(::bof3::Img(ObjTrio_addr) + 0x298);
    if (x < at::kTrio2Line) return false;
    B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0xFF;
    B(::bof3::Img(effect_4a::at::kCounterC.v)) = 0;
    B(::bof3::Img(at::kCounterD.v)) = 0;
    MoveScript_PartyRecords[0x65] = 0x14;
    return true;
}

}  // namespace

// original 0x487C10 (Effect_KindHandlers[0x82], hidden in 0x487BF0): jmp
// [EffectKind82_States + +1 * 4].
extern "C" void __cdecl EffectKind82_Run(void) {
    Dispatch("EffectKind82_Run", AddressOf(EffectKind82_States), EffectKind82_States_count, 1);
}

// original 0x487C30 (state 0, and entry 0 of the next table 0x654CE8): +9 0, +1
// 1, +2 0.
extern "C" void __cdecl EffectKind82_Start(void) {
    S()[9] = 0;
    S()[1] = 1;
    S()[2] = 0;
}

// original 0x487C50 (state 1): +1 0x16 when record 0 has reached the leader,
// 0x17 when 0x90384A is 0x80; then by 0x903849 - 3: +1 6, 4: +1 4, 5: +1 2
// (three, four or five pushes), 0xFF: a tail jump to Effect_Release (MSVC's switch
// through the byte table 0x487CD4 and the jump table 0x487CC0 in the code).
extern "C" void __cdecl EffectKind82_Wait(void) {
    unsigned char* s = S();
    if (ObjectReached()) {
        s[1] = 0x16;
        s = S();
    }
    if (B(::bof3::Img(effect_3d::at::kCounterC.v)) == 0x80) {
        s[1] = 0x17;
        s = S();
    }
    switch (B(::bof3::Img(effect_3d::at::kCounterB.v))) {
    case 3: s[1] = 6; return;
    case 4: s[1] = 4; return;
    case 5: s[1] = 2; return;
    case 0xFF: SH_CALL(Effect_Release)(); return;
    default: return;
    }
}

// original 0x487DE0 (state 2, a start no list of the cut has): a push, +1 3.
extern "C" void __cdecl EffectKind82_Push2(void) { Push(3); }

// original 0x487E20 (state 3): reached: +1 0xD; else +1 4.
extern "C" void __cdecl EffectKind82_Check3(void) { Check(4); }

// original 0x487E60 (state 4): a push, +1 5.
extern "C" void __cdecl EffectKind82_Push4(void) { Push(5); }

// original 0x487EA0 (state 5): reached: +1 0xD; else +1 6.
extern "C" void __cdecl EffectKind82_Check5(void) { Check(6); }

// original 0x487EE0 (state 6): a push, +1 7.
extern "C" void __cdecl EffectKind82_Push6(void) { Push(7); }

// original 0x487F20 (state 7): reached: +1 0xD; else +1 8.
extern "C" void __cdecl EffectKind82_Check7(void) { Check(8); }

// original 0x487F60 (state 8): a push, +1 9.
extern "C" void __cdecl EffectKind82_Push8(void) { Push(9); }

// original 0x487FA0 (state 9): reached: +1 0xD; else +1 0xA.
extern "C" void __cdecl EffectKind82_Check9(void) { Check(0xA); }

// original 0x487FE0 (state 10): a push, +1 0xB (E4A's state 11 follows).
extern "C" void __cdecl EffectKind82_Push10(void) { Push(0xB); }

// original 0x488020 (EffectKind82_States[11], hidden in 0x487BF0): record 0
// reached the leader: +1 0xD, +9 0; else +1 0xC.
extern "C" void __cdecl EffectKind82_Check11(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0x34, 0xC); }

// original 0x488060 (state 12): +1 0; 0x903849 0 when it is 0xFF or 0x90384A
// is 0x80.
extern "C" void __cdecl EffectKind82_Restart(void) {
    S()[1] = 0;
    if (B(::bof3::Img(effect_4a::at::kCounterB.v)) == 0xFF || B(::bof3::Img(effect_4a::at::kCounterC.v)) == 0x80) B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0;
}

// original 0x488090 (state 13): record 0's z + 0x4000 twice, +1 0xE, +9 8.
extern "C" void __cdecl EffectKind82_Nudge13(void) { Nudge(rv::Bytes(Sprite_Objects), 0x38, 2, 0x4000u, 0xE, 8); }

// original 0x4880D0 (state 15): record 0's z - 0x4000 four times, +1 0x10, +9 8.
extern "C" void __cdecl EffectKind82_Nudge15(void) { Nudge(rv::Bytes(Sprite_Objects), 0x38, 4, 0xFFFFC000u, 0x10, 8); }

// original 0x488110 (state 17): record 0's z + 0x4000 twice, +1 0x12.
extern "C" void __cdecl EffectKind82_Nudge17(void) { Nudge(rv::Bytes(Sprite_Objects), 0x38, 2, 0x4000u, 0x12, -1); }

// original 0x488150 (state 18): +2 up one; +1 0x14 when it was below 0xF,
// else 0xD.
extern "C" void __cdecl EffectKind82_Count18(void) {
    unsigned char* const s = S();
    const unsigned char was = s[2];
    s[2] = static_cast<unsigned char>(was + 1);
    S()[1] = was < 0xF ? 0x14 : 0xD;
}

// original 0x488180 (state 20): ended (0x903849 0xFF or 0x90384A 0x80): +1
// 0x17, +2 0; else 0x903849 0, +1 0, +2 0.
extern "C" void __cdecl EffectKind82_Again(void) { Again(); }

// original 0x4881C0 (state 22): Field_State the third member's object; its x
// at 0x34 cells or past: 0x903849 0xFF, the step 0x14; else 0x903849 0xFE, the
// step 0x1E; 0x90384A and 0x90384B 0 either way; a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind82_End(void) {
    if (!PastLine()) {
        B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0xFE;
        B(::bof3::Img(effect_4a::at::kCounterC.v)) = 0;
        B(::bof3::Img(at::kCounterD.v)) = 0;
        MoveScript_PartyRecords[0x65] = 0x1E;
    }
    SH_CALL(Effect_Release)();
}

// original 0x488220 (Effect_KindHandlers[0x83], hidden in 0x487BF0): jmp
// [EffectKind83_States + +1 * 4].
extern "C" void __cdecl EffectKind83_Run(void) {
    DispatchEffect4a("EffectKind83_Run", AddressOf(EffectKind83_States), EffectKind83_States_count);
}

// original 0x488240 (state 1): +1 0x16 when record 0 (not 1) has reached the
// leader, 0x17 when 0x90384A is 0x80; then by 0x903849 - 3: +1 6, 4: +1 4, 5: +1
// 2, 0xFF: a tail jump to Effect_Release (MSVC's switch through the byte table
// 0x4882C4 and the jump table 0x4882B0 in the code), else nothing.
extern "C" void __cdecl EffectKind83_Wait(void) {
    unsigned char* s = S();
    if (Reached(::bof3::Img(Sprite_Objects_addr) + 0x34)) {
        s[1] = 0x16;
        s = S();
    }
    if (B(::bof3::Img(effect_4a::at::kCounterC.v)) == 0x80) {
        s[1] = 0x17;
        s = S();
    }
    switch (B(::bof3::Img(effect_4a::at::kCounterB.v))) {
    case 3: s[1] = 6; return;
    case 4: s[1] = 4; return;
    case 5: s[1] = 2; return;
    case 0xFF: SH_CALL(Effect_Release)(); return;
    default: return;
    }
}

// original 0x4883D0 (state 2, a start no list of the cut has): record 1's x
// + 0x4000 four times (a cell), +1 3.
extern "C" void __cdecl EffectKind83_Push2(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x34, 4, 0x4000u, 3, -1); }

// original 0x488410 (state 3): record 1 reached: +1 0xD, +9 0; else +1 4.
extern "C" void __cdecl EffectKind83_Check3(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0xd8, 4); }

// original 0x488450 (state 4): a push, +1 5.
extern "C" void __cdecl EffectKind83_Push4(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x34, 4, 0x4000u, 5, -1); }

// original 0x488490 (state 5): reached: +1 0xD; else +1 6.
extern "C" void __cdecl EffectKind83_Check5(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0xd8, 6); }

// original 0x4884D0 (state 6): a push, +1 7.
extern "C" void __cdecl EffectKind83_Push6(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x34, 4, 0x4000u, 7, -1); }

// original 0x488510 (state 7): reached: +1 0xD; else +1 8.
extern "C" void __cdecl EffectKind83_Check7(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0xd8, 8); }

// original 0x488550 (state 8): a push, +1 9.
extern "C" void __cdecl EffectKind83_Push8(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x34, 4, 0x4000u, 9, -1); }

// original 0x488590 (state 9): reached: +1 0xD; else +1 0xA.
extern "C" void __cdecl EffectKind83_Check9(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0xd8, 0xA); }

// original 0x4885D0 (state 10): a push, +1 0xB.
extern "C" void __cdecl EffectKind83_Push10(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x34, 4, 0x4000u, 0xB, -1); }

// original 0x488610 (state 11): reached: +1 0xD; else +1 0xC.
extern "C" void __cdecl EffectKind83_Check11(void) { CheckEffect4a(::bof3::Img(Sprite_Objects_addr) + 0xd8, 0xC); }

// original 0x488650 (state 13): record 1's z + 0x4000 twice, +1 0xE, +9 8.
extern "C" void __cdecl EffectKind83_Nudge13(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x38, 2, 0x4000u, 0xE, 8); }

void EffectKind82FromEffect3d_Inject() {
    BOF3_INJECT(EffectKind82_Run);
    BOF3_INJECT(EffectKind82_Start);
    BOF3_INJECT(EffectKind82_Wait);
    BOF3_INJECT(EffectKind82_Push2);
    BOF3_INJECT(EffectKind82_Check3);
    BOF3_INJECT(EffectKind82_Push4);
    BOF3_INJECT(EffectKind82_Check5);
    BOF3_INJECT(EffectKind82_Push6);
    BOF3_INJECT(EffectKind82_Check7);
    BOF3_INJECT(EffectKind82_Push8);
    BOF3_INJECT(EffectKind82_Check9);
    BOF3_INJECT(EffectKind82_Push10);
}

void EffectKind82FromEffect4a_Inject() {
    BOF3_INJECT(EffectKind82_Check11);
    BOF3_INJECT(EffectKind82_Restart);
    BOF3_INJECT(EffectKind82_Nudge13);
    BOF3_INJECT(EffectKind82_Nudge15);
    BOF3_INJECT(EffectKind82_Nudge17);
    BOF3_INJECT(EffectKind82_Count18);
    BOF3_INJECT(EffectKind82_Again);
    BOF3_INJECT(EffectKind82_End);
    BOF3_INJECT(EffectKind83_Run);
    BOF3_INJECT(EffectKind83_Wait);
    BOF3_INJECT(EffectKind83_Push2);
    BOF3_INJECT(EffectKind83_Check3);
    BOF3_INJECT(EffectKind83_Push4);
    BOF3_INJECT(EffectKind83_Check5);
    BOF3_INJECT(EffectKind83_Push6);
    BOF3_INJECT(EffectKind83_Check7);
    BOF3_INJECT(EffectKind83_Push8);
    BOF3_INJECT(EffectKind83_Check9);
    BOF3_INJECT(EffectKind83_Push10);
    BOF3_INJECT(EffectKind83_Check11);
    BOF3_INJECT(EffectKind83_Nudge13);
}

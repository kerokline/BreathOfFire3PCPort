// Moved here by tools/move_funcs.py from effect_4a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_84.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

using Handler = scenario_harness::Handler;

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

// +9 down one; +1 `next` when it was 0.
void Hold(unsigned next) {
    unsigned char* const s = S();
    const unsigned char was = s[9];
    s[9] = static_cast<unsigned char>(was - 1);
    if (was == 0) S()[1] = static_cast<unsigned char>(next);
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

bool CountingHeld() {
    const U held = Input_Held;
    return held == at::kHeldA || held == at::kHeldB || held == at::kHeldC;
}

}  // namespace

// original 0x4886F0 (state 16 of kinds 0x82 and 0x83): +9 down; +1 0x11 when
// it was 0.
extern "C" void __cdecl EffectKind83_Hold16(void) { Hold(0x11); }

// original 0x488710 (state 17): record 1's z + 0x4000 twice, +1 0x12.
extern "C" void __cdecl EffectKind83_Nudge17(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x38, 2, 0x4000u, 0x12, -1); }

// original 0x488750 (state 18): +2 up one; when it was 0xF or more +1 0xD;
// else kind 0x82's state 20 in line (EffectKind83_States[20] is BareRet).
extern "C" void __cdecl EffectKind83_Count18(void) {
    unsigned char* const s = S();
    const unsigned char was = s[2];
    s[2] = static_cast<unsigned char>(was + 1);
    if (was >= 0xF) {
        S()[1] = 0xD;
        return;
    }
    Again();
}

// original 0x4887B0 (state 23 of kinds 0x82 and 0x83): Field_State the leader;
// +0x34 the leader's x less record 0's; the step 0x1E; a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind83_Finish(void) {
    const U object_x = static_cast<std::uint32_t>(Sprite_Objects[0].x);
    const U leader_x = static_cast<std::uint32_t>(ObjTrio[0].x);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    Field_State = rv::Bytes(ObjTrio);
    so->x = leader_x - object_x;
    MoveScript_PartyRecords[0x65] = 0x1E;
    SH_CALL(Effect_Release)();
}

// original 0x4887E0 (Effect_KindHandlers[0x84], hidden in 0x487BF0): jmp
// [EffectKind84_States + +1 * 4].
extern "C" void __cdecl EffectKind84_Run(void) {
    DispatchEffect4a("EffectKind84_Run", AddressOf(EffectKind84_States), EffectKind84_States_count);
}

// original 0x488800 (state 0): +1 1 when Input_Held is 0x3000, 0x6000 or 0x2000.
extern "C" void __cdecl EffectKind84_WaitPress(void) {
    if (CountingHeld()) S()[1] = 1;
}

// original 0x488830 (state 1): 0x903849 0xFE: Effect_Release (and on). The
// third member past the line: Effect_Release, done. Else 0x903849 the push
// count EffectKind84_PushCounts[Rand & 0xF], +1 2.
extern "C" void __cdecl EffectKind84_Pick(void) {
    if (B(::bof3::Img(at::kCounterB.v)) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    const U r = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
    unsigned char* const s = S();
    B(::bof3::Img(at::kCounterB.v)) = B(::bof3::Img(at::kPushCounts.v) + r);
    s[1] = 2;
}

// original 0x4888B0 (state 2): the stop and the line as state 1. Else, the
// pushes done (0x903849 0): +9 EffectKind84_Waits[Rand & 0xF], +1 3; else
// each frame a counting held word: 0x90384A up one, and when it was 0x10 or
// more 0x90384A 0x80 and Effect_Release.
extern "C" void __cdecl EffectKind84_Mash(void) {
    if (B(::bof3::Img(at::kCounterB.v)) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (B(::bof3::Img(at::kCounterB.v)) == 0) {
        const U r = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        unsigned char* const s = S();
        s[9] = B(::bof3::Img(at::kWaits.v) + r);
        S()[1] = 3;
        return;
    }
    if (!CountingHeld()) return;
    const unsigned char was = B(::bof3::Img(at::kCounterC.v));
    B(::bof3::Img(at::kCounterC.v)) = static_cast<unsigned char>(was + 1);
    if (was < 0x10) return;
    B(::bof3::Img(at::kCounterC.v)) = 0x80;
    SH_CALL(Effect_Release)();
}

// original 0x488970 (state 3): the stop and the line as state 1 (the line a
// tail jump). Else +9 down; when it was 0 the step word 0x8034E6 0 and +1 1.
extern "C" void __cdecl EffectKind84_Pause(void) {
    if (B(::bof3::Img(at::kCounterB.v)) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* const s = S();
    const unsigned char was = s[9];
    s[9] = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    unsigned char* const t = S();
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0);
    t[1] = 1;
}

void EffectKind84_Inject() {
    BOF3_INJECT(EffectKind83_Hold16);
    BOF3_INJECT(EffectKind83_Nudge17);
    BOF3_INJECT(EffectKind83_Count18);
    BOF3_INJECT(EffectKind83_Finish);
    BOF3_INJECT(EffectKind84_Run);
    BOF3_INJECT(EffectKind84_WaitPress);
    BOF3_INJECT(EffectKind84_Pick);
    BOF3_INJECT(EffectKind84_Mash);
    BOF3_INJECT(EffectKind84_Pause);
}

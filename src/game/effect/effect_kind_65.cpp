// Moved here by tools/move_funcs.py from effect_3b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_65.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

unsigned char& B(U a) { return *At(a); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp / call
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original goes
// through the dword after - the next table.
void Through(const char* who, U table, unsigned entries, unsigned index, unsigned byte) {
    if (index >= entries)
        bof3::Fatal("%s: +%u is %u, past the %u entries of 0x%X - the original goes through the dword after "
                    "(docs/effect_3b.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * index)))();
}

void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    Through(who, table, entries, Sprite_Current[byte], byte);
}

// Effect_FindFree's record (the answer's low byte, as the originals widen it):
// a Fatal past the twenty - the original writes record (answer * 0x80) past the
// pool, 0xFF included where it does not test for none.
unsigned char* Spawned(const char* who, U answer) {
    const unsigned k = answer & 0xFF;
    if (k >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes record %u past "
                    "the pool (docs/effect_3b.md section 7)",
                    who, k, k);
    return rv::Bytes(&Effect_Objects[k]);
}

}  // namespace

// original 0x4828B0 (Effect_KindHandlers[0x65], hidden in 0x482740): jmp
// [EffectKind65_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind65_Run(void) {
    Dispatch("EffectKind65_Run", AddressOf(EffectKind65_States), EffectKind65_States_count, 1);
}

// original 0x4828D0 (state 0): Field_Request 3: +1 = 1.
extern "C" void __cdecl EffectKind65_WaitRequest(void) {
    if (Field_Request == 3) S()[1] = 1;
}

// original 0x4828F0 (state 1): nothing while Field_Request is 3; else the dword
// 0x904134 not a multiple of 5 (unsigned): +1 = 0; a multiple:
// ScriptFlags_Set40 and +1 = 2.
extern "C" void __cdecl EffectKind65_Check(void) {
    if (Field_Request == 3) return;
    if (Game_BattleCount % 5 != 0) {
        S()[1] = 0;
        return;
    }
    SH_CALL(ScriptFlags_Set40)();
    S()[1] = 2;
}

// original 0x482930 (state 2): with Input_Held not 0: Sound_PlayEffect(0x202),
// story flag 0x4F toggled, +9 = 60, +1 = 3.
extern "C" void __cdecl EffectKind65_WaitInput(void) {
    if (Input_Held == 0) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(Flags_Toggle)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kKind65Flag);
    S()[9] = 0x3C;
    S()[1] = 3;
}

// original 0x482970 (state 3): +9 down; at 0 Msg_OpenScript(1), Field_Request =
// 2, +1 = 4; else MapView_Redraw = 2 and Camera_ShiftY += 4 * the signed byte
// EffectKind65_ShakeSteps[+9 & 3] (16 bits).
extern "C" void __cdecl EffectKind65_Shake(void) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const unsigned left = s[9];
    if (left == 0) {
        SH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        S()[1] = 4;
        return;
    }
    MapView_Redraw = 2;
    const U step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kShakeSteps.v) + (left & 3)))));
    Camera_ShiftY = static_cast<short>(static_cast<U>(static_cast<unsigned short>(Camera_ShiftY)) + (step << 2));
}

// original 0x4829D0 (state 4): with Field_Request no longer 2:
// ScriptFlags_Clear40, Sound_PlayEffect(0x206), +1 = 0.
extern "C" void __cdecl EffectKind65_Close(void) {
    if (Field_Request == 2) return;
    SH_CALL(ScriptFlags_Clear40)();
    SH_CALL(Sound_PlayEffect)(0x206);
    S()[1] = 0;
}

// original 0x482A00 (Effect_KindHandlers[0x67], hidden in 0x482740): jmp
// [EffectKind67_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind67_Run(void) {
    Dispatch("EffectKind67_Run", AddressOf(EffectKind67_States), EffectKind67_States_count, 1);
}

// original 0x482A20 (state 1): Effect_FindFree's byte to 0x903850; not 0xFF:
// that record +0 = 1, kind +5 = 0x13, +0x64 = -0x386, +0x68 = the s16
// Camera_Angles + 2, +0x6C = 0xE2, +9 = 0x1E. Its own +1 is left: one a frame.
extern "C" void __cdecl EffectKind67_SpawnKind13(void) {
    const U answer = SH_CALL(Effect_FindFree)();
    at::kScale.b_00 = static_cast<unsigned char>(answer);
    if ((answer & 0xFF) == 0xFF) return;
    const U angle = S16(static_cast<std::uint32_t>(static_cast<std::uint16_t>(Camera_Angles[1])));
    unsigned char* const e = Spawned("EffectKind67_SpawnKind13", static_cast<std::uint32_t>(at::kScale.d_00));
    e[0] = 1;
    e[5] = 0x13;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(e)->angle[0], 0xFFFFFC7Au);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(e)->angle[1], angle);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(e)->angle[2], 0xE2);
    e[9] = 0x1E;
}

void EffectKind65_Inject() {
    BOF3_INJECT(EffectKind65_Run);
    BOF3_INJECT(EffectKind65_WaitRequest);
    BOF3_INJECT(EffectKind65_Check);
    BOF3_INJECT(EffectKind65_WaitInput);
    BOF3_INJECT(EffectKind65_Shake);
    BOF3_INJECT(EffectKind65_Close);
    BOF3_INJECT(EffectKind67_Run);
    BOF3_INJECT(EffectKind67_SpawnKind13);
}

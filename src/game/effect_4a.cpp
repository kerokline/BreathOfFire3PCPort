// Round thirteen group E4A (docs/effect_4a.md): the 48 functions of
// analysis/round13_cut.tsv's group E4A and three starts the cut does not list
// (kind 0x83's state 2 0x4883D0, kind 0x85's shared draw tail 0x488B90 and kind
// 0x86's dispatcher 0x488BE0), each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x82   (states 11..23; 0..10 are E3D's) Sprite_Objects record 0, once
//               it has reached the leader, nudged in z (+0.5, -1, +0.5 cells
//               with eight-frame holds), then restarted or ended
//   kind 0x83   kind 0x82 again for record 1: pushed toward the leader by
//               0x903849's count, nudged, restarted or ended
//   kind 0x84   the count's driver: waits for one of three held-button words,
//               picks the next push count (EffectKind84_PushCounts by Rand),
//               counts the presses into 0x90384A (0x80 at 17); ends when the
//               third member's x passes 0x34 cells or 0x903849 is 0xFE
//   kind 0x85   a run of messages 0x33..0x36 under a full-screen tile that
//               brightens between them; then waits for the counter at 0x35
//   kind 0x86   two free sprites placed by EventOp_0x, one cell either side in
//               z, slid back over 32 frames, tinted brighter, then freed
//   kind 0x87   E4B's records in EffectKind30_Shards set up, run to their end,
//               the draw pass flags kept and put back
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes Sprite_Objects past its 30 records by a byte
// of the record, ours aborts with a message (docs/effect_4a.md section 7).
#include "game/effect_4a.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
std::int32_t SL(U a) { return Long(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table, a null or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4a.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(P(table + 4 * state))))();
}

// Sprite_Objects record `index` (a byte of the effect record, unchecked by the
// original: past the 30 records it writes the draw records 0x7E01C0.. and on).
unsigned char* Object(const char* who, unsigned index) {
    if (index >= at::kSpriteCount)
        bof3::Fatal("%s: Sprite_Objects index %u, past its %u records - the original writes past them "
                    "(docs/effect_4a.md section 7)",
                    who, index, at::kSpriteCount);
    return Sprite_Objects + at::kSpriteStride * index;
}

// Field_State the leader; true when the dword at `x` (a record's +0x34) is the
// leader's x or past it (signed).
bool Reached(U x) {
    const std::int32_t object_x = SL(x);
    const std::int32_t leader_x = SL(at::kLeaderX);
    Field_State = ObjTrio;
    return object_x >= leader_x;
}

// +1 0xD and +9 0 when the record at `x` has reached the leader; else +1 `next`.
void Check(U x, unsigned next) {
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
    if (B(at::kCounterB) == 0xFF || B(at::kCounterC) == 0x80) {
        S()[1] = 0x17;
        S()[2] = 0;
        return;
    }
    unsigned char* const s = S();
    B(at::kCounterB) = 0;
    s[1] = 0;
    S()[2] = 0;
}

// Field_State the third member's object; its x at 0x34 cells or past it
// (signed): true, with 0x903849 0xFF, 0x90384A / B 0 and the step 0x14.
bool PastLine() {
    const std::int32_t x = SL(at::kObjTrio2X);
    Field_State = P(at::kObjTrio2);
    if (x < at::kTrio2Line) return false;
    B(at::kCounterB) = 0xFF;
    B(at::kCounterC) = 0;
    B(at::kCounterD) = 0;
    B(at::kStep) = 0x14;
    return true;
}

bool CountingHeld() {
    const U held = Input_Held;
    return held == at::kHeldA || held == at::kHeldB || held == at::kHeldC;
}

}  // namespace

// ===========================================================================
// Kind 0x82, states 11..23 (EffectKind82_States, E3D's; 0..10 E3D's)
// ===========================================================================

// original 0x488020 (EffectKind82_States[11], hidden in 0x487BF0): record 0
// reached the leader: +1 0xD, +9 0; else +1 0xC.
extern "C" void __cdecl EffectKind82_Check11(void) { Check(at::kObject0X, 0xC); }

// original 0x488060 (state 12): +1 0; 0x903849 0 when it is 0xFF or 0x90384A
// is 0x80.
extern "C" void __cdecl EffectKind82_Restart(void) {
    S()[1] = 0;
    if (B(at::kCounterB) == 0xFF || B(at::kCounterC) == 0x80) B(at::kCounterB) = 0;
}

// original 0x488090 (state 13): record 0's z + 0x4000 twice, +1 0xE, +9 8.
extern "C" void __cdecl EffectKind82_Nudge13(void) { Nudge(Sprite_Objects, 0x38, 2, 0x4000u, 0xE, 8); }

// original 0x4880D0 (state 15): record 0's z - 0x4000 four times, +1 0x10, +9 8.
extern "C" void __cdecl EffectKind82_Nudge15(void) { Nudge(Sprite_Objects, 0x38, 4, 0xFFFFC000u, 0x10, 8); }

// original 0x488110 (state 17): record 0's z + 0x4000 twice, +1 0x12.
extern "C" void __cdecl EffectKind82_Nudge17(void) { Nudge(Sprite_Objects, 0x38, 2, 0x4000u, 0x12, -1); }

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
        B(at::kCounterB) = 0xFE;
        B(at::kCounterC) = 0;
        B(at::kCounterD) = 0;
        B(at::kStep) = 0x1E;
    }
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x83: EffectKind83_States (24)
// ===========================================================================

// original 0x488220 (Effect_KindHandlers[0x83], hidden in 0x487BF0): jmp
// [EffectKind83_States + +1 * 4].
extern "C" void __cdecl EffectKind83_Run(void) {
    Dispatch("EffectKind83_Run", AddressOf(EffectKind83_States), EffectKind83_States_count);
}

// original 0x488240 (state 1): +1 0x16 when record 0 (not 1) has reached the
// leader, 0x17 when 0x90384A is 0x80; then by 0x903849 - 3: +1 6, 4: +1 4, 5: +1
// 2, 0xFF: a tail jump to Effect_Release (MSVC's switch through the byte table
// 0x4882C4 and the jump table 0x4882B0 in the code), else nothing.
extern "C" void __cdecl EffectKind83_Wait(void) {
    unsigned char* s = S();
    if (Reached(at::kObject0X)) {
        s[1] = 0x16;
        s = S();
    }
    if (B(at::kCounterC) == 0x80) {
        s[1] = 0x17;
        s = S();
    }
    switch (B(at::kCounterB)) {
    case 3: s[1] = 6; return;
    case 4: s[1] = 4; return;
    case 5: s[1] = 2; return;
    case 0xFF: SH_CALL(Effect_Release)(); return;
    default: return;
    }
}

// original 0x4883D0 (state 2, a start no list of the cut has): record 1's x
// + 0x4000 four times (a cell), +1 3.
extern "C" void __cdecl EffectKind83_Push2(void) { Nudge(P(at::kObject1), 0x34, 4, 0x4000u, 3, -1); }
// original 0x488410 (state 3): record 1 reached: +1 0xD, +9 0; else +1 4.
extern "C" void __cdecl EffectKind83_Check3(void) { Check(at::kObject1X, 4); }
// original 0x488450 (state 4): a push, +1 5.
extern "C" void __cdecl EffectKind83_Push4(void) { Nudge(P(at::kObject1), 0x34, 4, 0x4000u, 5, -1); }
// original 0x488490 (state 5): reached: +1 0xD; else +1 6.
extern "C" void __cdecl EffectKind83_Check5(void) { Check(at::kObject1X, 6); }
// original 0x4884D0 (state 6): a push, +1 7.
extern "C" void __cdecl EffectKind83_Push6(void) { Nudge(P(at::kObject1), 0x34, 4, 0x4000u, 7, -1); }
// original 0x488510 (state 7): reached: +1 0xD; else +1 8.
extern "C" void __cdecl EffectKind83_Check7(void) { Check(at::kObject1X, 8); }
// original 0x488550 (state 8): a push, +1 9.
extern "C" void __cdecl EffectKind83_Push8(void) { Nudge(P(at::kObject1), 0x34, 4, 0x4000u, 9, -1); }
// original 0x488590 (state 9): reached: +1 0xD; else +1 0xA.
extern "C" void __cdecl EffectKind83_Check9(void) { Check(at::kObject1X, 0xA); }
// original 0x4885D0 (state 10): a push, +1 0xB.
extern "C" void __cdecl EffectKind83_Push10(void) { Nudge(P(at::kObject1), 0x34, 4, 0x4000u, 0xB, -1); }
// original 0x488610 (state 11): reached: +1 0xD; else +1 0xC.
extern "C" void __cdecl EffectKind83_Check11(void) { Check(at::kObject1X, 0xC); }

// original 0x433640 (EffectKind83_States[12], and the stack table of the
// battle's BattleFx_ActorWatch 0x433460, its state 2; hidden in 0x432F10): +1 0.
extern "C" void __cdecl Sprite_StateRestart(void) { S()[1] = 0; }

// original 0x488650 (state 13): record 1's z + 0x4000 twice, +1 0xE, +9 8.
extern "C" void __cdecl EffectKind83_Nudge13(void) { Nudge(P(at::kObject1), 0x38, 2, 0x4000u, 0xE, 8); }

// original 0x488690 (state 14 of kinds 0x82 and 0x83): +9 down; +1 0xF when it
// was 0.
extern "C" void __cdecl EffectKind83_Hold14(void) { Hold(0xF); }

// original 0x4886B0 (state 15): record 1's z - 0x4000 four times, +1 0x10, +9 8.
extern "C" void __cdecl EffectKind83_Nudge15(void) { Nudge(P(at::kObject1), 0x38, 4, 0xFFFFC000u, 0x10, 8); }

// original 0x4886F0 (state 16 of kinds 0x82 and 0x83): +9 down; +1 0x11 when
// it was 0.
extern "C" void __cdecl EffectKind83_Hold16(void) { Hold(0x11); }

// original 0x488710 (state 17): record 1's z + 0x4000 twice, +1 0x12.
extern "C" void __cdecl EffectKind83_Nudge17(void) { Nudge(P(at::kObject1), 0x38, 2, 0x4000u, 0x12, -1); }

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
    const U object_x = UL(At(at::kObject0X));
    const U leader_x = UL(At(at::kLeaderX));
    unsigned char* const s = S();
    Field_State = ObjTrio;
    SetUL(s + 0x34, leader_x - object_x);
    B(at::kStep) = 0x1E;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x84: EffectKind84_States (5)
// ===========================================================================

// original 0x4887E0 (Effect_KindHandlers[0x84], hidden in 0x487BF0): jmp
// [EffectKind84_States + +1 * 4].
extern "C" void __cdecl EffectKind84_Run(void) {
    Dispatch("EffectKind84_Run", AddressOf(EffectKind84_States), EffectKind84_States_count);
}

// original 0x488800 (state 0): +1 1 when Input_Held is 0x3000, 0x6000 or 0x2000.
extern "C" void __cdecl EffectKind84_WaitPress(void) {
    if (CountingHeld()) S()[1] = 1;
}

// original 0x488830 (state 1): 0x903849 0xFE: Effect_Release (and on). The
// third member past the line: Effect_Release, done. Else 0x903849 the push
// count EffectKind84_PushCounts[Rand & 0xF], +1 2.
extern "C" void __cdecl EffectKind84_Pick(void) {
    if (B(at::kCounterB) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    const U r = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
    unsigned char* const s = S();
    B(at::kCounterB) = B(at::kPushCounts + r);
    s[1] = 2;
}

// original 0x4888B0 (state 2): the stop and the line as state 1. Else, the
// pushes done (0x903849 0): +9 EffectKind84_Waits[Rand & 0xF], +1 3; else
// each frame a counting held word: 0x90384A up one, and when it was 0x10 or
// more 0x90384A 0x80 and Effect_Release.
extern "C" void __cdecl EffectKind84_Mash(void) {
    if (B(at::kCounterB) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (B(at::kCounterB) == 0) {
        const U r = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        unsigned char* const s = S();
        s[9] = B(at::kWaits + r);
        S()[1] = 3;
        return;
    }
    if (!CountingHeld()) return;
    const unsigned char was = B(at::kCounterC);
    B(at::kCounterC) = static_cast<unsigned char>(was + 1);
    if (was < 0x10) return;
    B(at::kCounterC) = 0x80;
    SH_CALL(Effect_Release)();
}

// original 0x488970 (state 3): the stop and the line as state 1 (the line a
// tail jump). Else +9 down; when it was 0 the step word 0x8034E6 0 and +1 1.
extern "C" void __cdecl EffectKind84_Pause(void) {
    if (B(at::kCounterB) == 0xFE) SH_CALL(Effect_Release)();
    if (PastLine()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* const s = S();
    const unsigned char was = s[9];
    s[9] = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    unsigned char* const t = S();
    SetWord(At(at::kStepWord), 0);
    t[1] = 1;
}

// ===========================================================================
// Kind 0x85: EffectKind85_States (5)
// ===========================================================================

// original 0x4889E0 (Effect_KindHandlers[0x85], hidden in 0x487BF0): jmp
// [EffectKind85_States + +1 * 4].
extern "C" void __cdecl EffectKind85_Run(void) {
    Dispatch("EffectKind85_Run", AddressOf(EffectKind85_States), EffectKind85_States_count);
}

// original 0x488A00 (state 0): the tile's colour +0x5D..+0x5F 0, the message
// +0xB 0x33, Msg_OpenScript(+0xB), Field_Request 2, +1 1.
extern "C" void __cdecl EffectKind85_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[0xB] = 0x33;
    SH_CALL(Msg_OpenScript)(S()[0xB]);
    unsigned char* const s = S();
    Field_Request = 2;
    s[1] = 1;
}

// original 0x488B90 (the tail states 1..4 jump to; its own frame and ret, a
// start no list of the cut has): Sprite_Objects record +6's +0x29 2 and its
// screen point (Sprite_UpdateScreen), the leader's likewise; Sprite_Current
// put back.
extern "C" void __cdecl EffectKind85_ShowObjects(void) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = Object("EffectKind85_ShowObjects", saved[6]);
    Sprite_Current[0x29] = 2;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = ObjTrio;
    ObjTrio[0x29] = 2;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = saved;
}

namespace {
// The end of states 1..4: the tile (E4D's 0x48CA90), then a tail jump to
// EffectKind85_ShowObjects.
void FadeAndShow() {
    SH_AT(Handler, at::kFade)();
    SH_CALL(EffectKind85_ShowObjects)();
}
}  // namespace

// original 0x488A50 (state 1): the message closed (Field_Request not 2): +9
// 0x1E, +1 2. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_WaitMessage(void) {
    if (Field_Request != 2) {
        S()[9] = 0x1E;
        S()[1] = 2;
    }
    FadeAndShow();
}

// original 0x488A80 (state 2): +9 down; not yet 0: the tile's colour +0x5D,
// +0x5E, +0x5F up 2 each; else +9 0x1E, +1 3. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_Brighten(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 2);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 2);
    } else {
        s[9] = 0x1E;
        S()[1] = 3;
    }
    FadeAndShow();
}

// original 0x488AE0 (state 3): +9 down; at 0 the message +0xB up one - at 0x37
// the colour 0xFF, +1 4 and the counter 0x903848 up one; else
// Msg_OpenScript(+0xB), Field_Request 2, +1 1. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_NextMessage(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const s = S();
    if (s[9] == 0) {
        s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
        unsigned char* const t = S();
        const unsigned char message = t[0xB];
        if (message == 0x37) {
            t[0x5F] = 0xFF;
            S()[0x5E] = 0xFF;
            S()[0x5D] = 0xFF;
            S()[1] = 4;
            B(at::kCounter) = static_cast<unsigned char>(B(at::kCounter) + 1);
        } else {
            SH_CALL(Msg_OpenScript)(message);
            unsigned char* const u = S();
            Field_Request = 2;
            u[1] = 1;
        }
    }
    FadeAndShow();
}

// original 0x488B70 (state 4): Effect_Release when the counter 0x903848 is
// 0x35; then the tile and the objects (the record's +6 read after the release).
extern "C" void __cdecl EffectKind85_Wait(void) {
    if (B(at::kCounter) == 0x35) SH_CALL(Effect_Release)();
    FadeAndShow();
}

// ===========================================================================
// Kind 0x86: EffectKind86_States (4)
// ===========================================================================

// original 0x488BE0 (Effect_KindHandlers[0x86], hidden in 0x487BF0; no list of
// the cut has it): jmp [EffectKind86_States + +1 * 4].
extern "C" void __cdecl EffectKind86_Run(void) {
    Dispatch("EffectKind86_Run", AddressOf(EffectKind86_States), EffectKind86_States_count);
}

// original 0x488C00 (state 0): two free sprites (Sprite_FindFree) into +3 and
// +4, each marked in use (+0 1) as found - none: nothing (the first freed
// again when the second is none). Both placed by EventOp_0x(0x654D88) with the
// count word 0x903850 the sprite; Sprite_Current put back; the first's z a
// cell less, the second's a cell more, +0 bit 5 and +0x5C 1 on both; tinted
// (Sprite_SetTint: the first (0xF, 0, 0, 1), the second (0, 0, 0xF, 1)); +9
// 0x20, +1 1.
extern "C" void __cdecl EffectKind86_Spawn(void) {
    static const char* const kWho = "EffectKind86_Spawn";
    const unsigned char first = SH_CALL(Sprite_FindFree)();
    S()[3] = first;
    if (S()[3] == 0xFF) return;
    Object(kWho, S()[3])[0] = 1;
    const unsigned char second = SH_CALL(Sprite_FindFree)();
    S()[4] = second;
    unsigned char* const s = S();
    if (s[4] == 0xFF) {
        Object(kWho, s[3])[0] = 0;
        return;
    }
    Object(kWho, s[4])[0] = 1;
    SetWord(At(at::kObjectCount), s[3]);
    SH_CALL(EventOp_0x)(P(at::kPlaceOp));
    SetWord(At(at::kObjectCount), s[4]);
    SH_CALL(EventOp_0x)(P(at::kPlaceOp));
    Sprite_Current = s;
    unsigned char* const a = Object(kWho, s[3]);
    SetUL(a + 0x38, UL(a + 0x38) + 0xFFFF0000u);
    unsigned char* const b = Object(kWho, s[4]);
    SetUL(b + 0x38, UL(b + 0x38) + 0x10000u);
    Object(kWho, s[3])[0] |= 0x20;
    Object(kWho, s[4])[0] |= 0x20;
    Object(kWho, s[4])[0x5C] = 1;
    Object(kWho, s[3])[0x5C] = 1;
    SH_CALL(Sprite_SetTint)(Object(kWho, s[3]), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(Object(kWho, s[4]), 0, 0, 0xF, 1);
    S()[9] = 0x20;
    S()[1] = 1;
}

// original 0x488DA0 (state 1): +9 not 0: the first sprite's z + 0x800, the
// second's - 0x800, +9 down; else +9 0xF, +1 2.
extern "C" void __cdecl EffectKind86_Slide(void) {
    static const char* const kWho = "EffectKind86_Slide";
    unsigned char* const s = S();
    if (s[9] == 0) {
        s[9] = 0xF;
        S()[1] = 2;
        return;
    }
    unsigned char* const a = Object(kWho, s[3]);
    SetUL(a + 0x38, UL(a + 0x38) + 0x800u);
    unsigned char* const b = Object(kWho, s[4]);
    SetUL(b + 0x38, UL(b + 0x38) + 0xFFFFF800u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x488E10 (state 2): +9 not 0: both sprites' +0x5D, +0x5E, +0x5F up
// 8 each, +9 down; else the counter 0x903848 up one, +1 3.
extern "C" void __cdecl EffectKind86_Tint(void) {
    static const char* const kWho = "EffectKind86_Tint";
    unsigned char* const s = S();
    if (s[9] == 0) {
        B(at::kCounter) = static_cast<unsigned char>(B(at::kCounter) + 1);
        s[1] = 3;
        return;
    }
    for (unsigned byte : {3u, 4u})
        for (unsigned k = 0x5D; k <= 0x5F; ++k) {
            unsigned char* const o = Object(kWho, s[byte]);
            o[k] = static_cast<unsigned char>(o[k] + 8);
        }
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x488EF0 (state 3): both sprites free (+0 0), their tints released
// (Sprite_ReleaseTint; the second's index read again after the first call);
// Effect_Release.
extern "C" void __cdecl EffectKind86_End(void) {
    static const char* const kWho = "EffectKind86_End";
    unsigned char* const s = S();
    Object(kWho, s[3])[0] = 0;
    Object(kWho, s[4])[0] = 0;
    SH_CALL(Sprite_ReleaseTint)(Object(kWho, s[3]));
    SH_CALL(Sprite_ReleaseTint)(Object(kWho, S()[4]));
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x87: EffectKind87_States (9)
// ===========================================================================

// original 0x488F60 (Effect_KindHandlers[0x87], hidden in 0x487BF0): jmp
// [EffectKind87_States + +1 * 4].
extern "C" void __cdecl EffectKind87_Run(void) {
    Dispatch("EffectKind87_Run", AddressOf(EffectKind87_States), EffectKind87_States_count);
}

// original 0x488F80 (state 0): +6 the draw pass flags (Draw_PassFlags' low
// byte), which become 0x1B; E4B's set-up 0x489030; 0x676280 0; +1 up one.
extern "C" void __cdecl EffectKind87_Start(void) {
    S()[6] = Draw_PassFlags;
    Draw_PassFlags = 0x1B;
    SH_AT(Handler, at::kKind87Setup)();
    unsigned char* const s = S();
    B(at::kKind87Flag) = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x488FB0 (state 1): E4B's 0x489220 answered 0 (al): E4B's 0x4891F0,
// Sound_PlayEffect(0x203), +1 up one.
extern "C" void __cdecl EffectKind87_Step1(void) {
    const unsigned char going = SH_AT(unsigned char (__cdecl*)(), at::kKind87Step)();
    if (going != 0) return;
    SH_AT(Handler, at::kKind87Reset)();
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x488FE0 (state 2): 0x489220 answered 0: +1 up one.
extern "C" void __cdecl EffectKind87_Step2(void) {
    const unsigned char going = SH_AT(unsigned char (__cdecl*)(), at::kKind87Step)();
    if (going == 0) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x489000 (state 3): the draw pass flags' low byte put back from +6,
// +1 up one (to states 4..7, 0x492750 each, which step +1 on).
extern "C" void __cdecl EffectKind87_Restore(void) {
    unsigned char* const s = S();
    Draw_PassFlags = s[6];
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x489020 (state 8): Effect_Release, then the step word 0x8034E6 0.
extern "C" void __cdecl EffectKind87_End(void) {
    SH_CALL(Effect_Release)();
    SetWord(At(at::kStepWord), 0);
}

void Effect4A_Inject() {
    if (bof3::WantsShadow("effect_4a")) effect_4a::SelfTest();
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
    BOF3_INJECT(Sprite_StateRestart);
    BOF3_INJECT(EffectKind83_Nudge13);
    BOF3_INJECT(EffectKind83_Hold14);
    BOF3_INJECT(EffectKind83_Nudge15);
    BOF3_INJECT(EffectKind83_Hold16);
    BOF3_INJECT(EffectKind83_Nudge17);
    BOF3_INJECT(EffectKind83_Count18);
    BOF3_INJECT(EffectKind83_Finish);
    BOF3_INJECT(EffectKind84_Run);
    BOF3_INJECT(EffectKind84_WaitPress);
    BOF3_INJECT(EffectKind84_Pick);
    BOF3_INJECT(EffectKind84_Mash);
    BOF3_INJECT(EffectKind84_Pause);
    BOF3_INJECT(EffectKind85_Run);
    BOF3_INJECT(EffectKind85_Start);
    BOF3_INJECT(EffectKind85_WaitMessage);
    BOF3_INJECT(EffectKind85_Brighten);
    BOF3_INJECT(EffectKind85_NextMessage);
    BOF3_INJECT(EffectKind85_Wait);
    BOF3_INJECT(EffectKind85_ShowObjects);
    BOF3_INJECT(EffectKind86_Run);
    BOF3_INJECT(EffectKind86_Spawn);
    BOF3_INJECT(EffectKind86_Slide);
    BOF3_INJECT(EffectKind86_Tint);
    BOF3_INJECT(EffectKind86_End);
    BOF3_INJECT(EffectKind87_Run);
    BOF3_INJECT(EffectKind87_Start);
    BOF3_INJECT(EffectKind87_Step1);
    BOF3_INJECT(EffectKind87_Step2);
    BOF3_INJECT(EffectKind87_Restore);
    BOF3_INJECT(EffectKind87_End);
}

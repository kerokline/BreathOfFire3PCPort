// Round thirteen group E1B: the effect engine's resident code 0x4672F0..0x46A5F2
// (analysis/round13_cut.tsv, group E1B), taken with the scenario harness in
// effect mode (scenario_harness.h, docs/scenario_harness.md section 8).
// docs/effect_1b.md has each function, every caller, the tables and the fuzz.
//
// Effect kind 0xF (Effect_KindHandlers[0xF], its dispatcher 0x466080 and states
// 0..25 are E1A's): states 26..28 open, show and close a message list with a
// toggle row; state 29 is a child effect run by sub-kind (+2) while record 6
// holds its state 0xE; states 30..35 are the six sub-kinds' step dispatchers
// themselves (by +3); 36..40 are sub-kind 0's steps.
//
//   EffectKind0F_ListOpen        0x4672F0  state 26: window, toggles, the list sliding in by +9; at 0 a kind-0x1A child
//   EffectKind0F_ListShow        0x4673B0  state 27: the same at rest
//   EffectKind0F_ListClose       0x467400  state 28: the list sliding out by +9; at 4 the next state by +0xB / +6
//   EffectKind0F_Child           0x4674F0  state 29: EffectKind0F_Children[+2] while record 6 is in state 0xE, else released
//   EffectKind0F_Child0 .. 5     0x467520, 0x4677F0, 0x467A60, 0x467D60, 0x468020, 0x468320: the sub-kinds' +3 dispatch
//   EffectKind0F_Child0Start     0x467540  two kind-0x1A children, the sprite placed
//   EffectKind0F_Child0Animate   0x467630  the animation script 0x653CCC stepped
//   EffectKind0F_Child0Second    0x4676A0  bank 0x21, placed, a counter's step set
//   EffectKind0F_Child0Tick      0x467750  the sprite script ticked, queued
//   EffectKind0F_Child0Back      0x467760  bank 0x47 again, one step back
//   EffectKind0F_Child1Start     0x467810  placed
//   EffectKind0F_Child1Meter     0x4678C0  the script 0x653CEC; a value bounced 0..0x60 or counted; a bar and a number
//   EffectKind0F_Child2Start     0x467A80  placed
//   EffectKind0F_Child2Gauge     0x467B10  the script 0x653D08; a gauge 0..0x18; a kind-0x1A child at a marked step
//   EffectKind0F_Child2Fade      0x467CA0  bank 0x2B, 0x40 frames
//   EffectKind0F_Child2Blink     0x467D30  counted down, drawn every other 8 frames, released at 0
//   EffectKind0F_Child3Start     0x467D80  placed
//   EffectKind0F_Child3Grid      0x467E10  a 4-row grid of marks (0x653D34), the lit cell stepped by +9
//   EffectKind0F_Child4Start     0x468040  placed, the cursor at (0x6A, 0x6A)
//   EffectKind0F_Child4Move      0x4680F0  the script 0x653D5C moves the cursor; the panel drawn
//   EffectKind0F_DrawCursorPanel 0x468210  (down): the cursor clamped and drawn with its range
//   EffectKind0F_Child5Start     0x468340  as Child4Start, +0xB = 0x37
//   EffectKind0F_Child5Move      0x468350  the script 0x653DAC; a count to 0x3C printed
//   EffectKind0F_DrawMessageList 0x468560  (x, y): a window of script-pool lines scrolled by +8 / +0x14
//   EffectKind0F_DrawListFrame   0x468840  (x, y): the list's sprite frame
//   Panel_DrawEdgeQuad           0x468950  (x, y, height, which): a textured quad of the frames' edges (FE1 calls it too)
//   EffectKind0F_DrawCountHeader 0x468A40  a sprite, a label, +0x3E + 1 printed
//   EffectKind0F_DrawToggles     0x468AC0  (x, y, bits): three toggle boxes and their labels by the bits
//   EffectKind0F_DrawToggle      0x468BB0  (x, y, lit): one toggle box
//   EffectKind0F_DrawItemsB      0x468C50  (x, y): the accessories of category 0xB, the chosen one lit
//   EffectKind0F_DrawScrollMark  0x468E50  (x, y, height): a textured quad
//   EffectKind0F_DrawItemsA      0x468F00  (x, y): the accessories of category 0xA, scrolled
//   EffectKind0F_DrawEquipped    0x469210  (x, y): two accessories and their icons, a message
//   EffectKind0F_DrawTwinFrame   0x469490  (x, y): two windows and their sprite frames
//   EffectKind0F_DrawItemFrame   0x469630  (x, y): a window and its sprite frame
//   Panel_DrawWindow             0x469750  (x, y, w, h, colour): the bevel and the edges (FE2's trade screen too)
//   Panel_DrawWindowBevel        0x469790  (x, y, w, h): two semi-transparent quads and a tile in the style's colour
//   Panel_DrawWindowEdges        0x469960  (x, y, w, h, colour): two three-point lines
//   EffectKind0F_DrawGlyph       0x469AD0  (clut, text, width, x): one glyph primitive at record 3's y
//   EffectKind92_Follow          0x46A3E0  kind 0x92: a sprite object moved by the record's step; released with it
//   EffectKind11_DrawShade       0x46A450  kind 0x11: a screen-wide black semi-transparent quad
//   EffectKind12_DrawGradient    0x46A500  kind 0x12: a screen-top gradient
//   EffectKind14_Run             0x46A5E0  kind 0x14: EffectKind14_States[+1]
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; the one divergence, DIV-0069 (the toggles' labels and the
// accessories' names under a Latin overlay, src/game/fishing_text.cpp), is armed
// only after every self-test, so the fuzz compares Capcom's. Sprite_Current is read afresh at each use, as
// the originals read [0x937F88] after every call (where one keeps it in a
// register across code with no call, the two are the same). Where an original
// indexes past one of its tables - a state table, an animation script, the
// accessories, the sprite objects, a record Effect_FindFree did not give - ours
// aborts with a message (docs/effect_1b.md section 7).
#include "game/effect_1b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1b_callees.h"
#include "game/fishing_text.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1b::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char* Sc() { return Sprite_Current; }
U ScKey() { return static_cast<U>(reinterpret_cast<std::uintptr_t>(Sprite_Current)); }
unsigned char& B(U a) { return *At(a); }
unsigned char Style() { return B(at::kStyle); }
const unsigned char* Text(U a) { return At(a); }
char* Print() { return reinterpret_cast<char*>(At(at::kPrint)); }
// A float stored as the originals' fild / fstp store it: every value here is a
// whole number well inside a float's 24 bits, so exact.
void F(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// A .data state table read in place: the index a byte of the record, unchecked
// in the original, which jumps through whatever follows the table past its
// length; ours aborts there. While the fuzz runs its entries are recorders.
using Handler = void (__cdecl*)();
Handler StepAt(U table, unsigned index, unsigned count, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past the %u entries of 0x%X (the original jumps through what follows)", who, index,
                    count, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}
// A byte of one of the image's scripts or tables: past its end the original
// reads what follows, and ours aborts there. The steps read "the record before
// +0xA", so with +0xA 0 the original reads up to four bytes BEFORE the script -
// the last pointer of the step table in front of it, image constants - and ours
// reads the same bytes (the steps' own start never gets there: +9 0 advances
// +0xA before the read; a record left mid-step does, docs/effect_1b.md section 7).
unsigned char ByteOf(U table, unsigned bytes, int index, const char* who) {
    if (index < -4 || (index >= 0 && static_cast<unsigned>(index) >= bytes))
        bof3::Fatal("%s: byte %d of the %u-byte table 0x%X (the original reads past it)", who, index, bytes,
                    (unsigned)table);
    return B(static_cast<U>(static_cast<std::int32_t>(table) + index));
}
std::uint16_t WordOf(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: word %u of the %u-word table 0x%X (the original reads past it)", who, index, count,
                    (unsigned)table);
    return Word(At(table + 2 * index));
}
// The record Effect_FindFree answered, written without the original checking
// the answer (0xFF, none free, would write 0x7E91E0.., past the pool): ours
// aborts on any index outside the 20.
unsigned char* Spawned(unsigned char k, const char* who) {
    if (k >= at::kEffectCount)
        bof3::Fatal("%s: Effect_FindFree answered %u, and the original writes that record unchecked (past the pool)",
                    who, (unsigned)k);
    return Effect_Objects + at::kEffectStride * k;
}
// A kind-0x1A record in state 0x10, as five of the states spawn it.
unsigned char* SpawnMark(unsigned char k, const char* who) {
    unsigned char* const r = Spawned(k, who);
    r[0] = 1;
    r[5] = 0x1A;
    r[1] = 0x10;
    return r;
}
// An accessory's category byte (+0x12 of NameTable_Accessories' 24-byte records).
unsigned char Category(unsigned char id, const char* who) {
    if (id >= at::kAccessoryCount)
        bof3::Fatal("%s: accessory id %u past the %u records (the original reads past NameTable_Accessories)", who,
                    (unsigned)id, at::kAccessoryCount);
    return B(at::kAccessoryCategory + at::kAccessoryStride * id);
}
const unsigned char* AccessoryName(unsigned char id) {
    return At(bof3::addr::NameTable_Accessories + at::kAccessoryStride * id);
}
// The characters a name is drawn to: the original's 8, or 12 under a Latin
// overlay (DIVERGENCE DIV-0069: the US name field; 8 cut "Wooden Rod" to "Wooden R").
int NameCount() { return static_cast<int>(FishingText_NameCount()); }
// A script-pool message: MessagePools + its u16 offset.
const unsigned char* Pool(unsigned id) { return At(at::kPools + Word(At(at::kPools + 2 * id))); }

// --- the callees by address (E1F's, E1A's, E1G's) ------------------------------------
void DrawMode(unsigned id, unsigned slot) { SH_AT(void (__cdecl*)(unsigned, unsigned), at::kDrawMode)(id, slot); }
void DrawSprite(unsigned id, unsigned slot, int x, int y) {
    SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, int, int), at::kDrawSprite)(id, slot, x, y);
}
void Call3(U address, U a, U b, U c) { SH_AT(void (__cdecl*)(U, U, U), address)(a, b, c); }
void Call4(U address, U a, U b, U c, U d) { SH_AT(void (__cdecl*)(U, U, U, U), address)(a, b, c, d); }
void Call5(U address, U a, U b, U c, U d, U e) { SH_AT(void (__cdecl*)(U, U, U, U, U), address)(a, b, c, d, e); }
void Call6(U address, U a, U b, U c, U d, U e, U f) {
    SH_AT(void (__cdecl*)(U, U, U, U, U, U), address)(a, b, c, d, e, f);
}
void DrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    SH_CALL(Text_DrawAt)(x, y, colour, count, text);
}

// The toggles' bits the list states hand EffectKind0F_DrawToggles: 1 << +6
// (the shift count masked to 5 bits, as shl masks it) once the leader's +3 is
// 3 or more, else none.
U ToggleBits() { return B(at::kLeader3) >= 3 ? 1u << (Sc()[6] & 31) : 0u; }

// The pose every sub-kind's first step gives the record (Sprite_* read it as a
// sprite): +0x24 = 0x80, +0x29, +0x2A, the screen place +0x2E / +0x30, and the
// counters and tint bytes cleared. The stores have no call between them, so
// their order is not observable.
void Place(unsigned char b29, unsigned char b2A, unsigned x, unsigned y) {
    unsigned char* const s = Sc();
    s[0x24] = 0x80;
    s[0x29] = b29;
    s[0x2A] = b2A;
    SetWord(s + 0x2E, x);
    SetWord(s + 0x30, y);
    s[0x48] = 0;
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
}

}  // namespace

// --- kind 0xF: the list states -------------------------------------------------------

// original 0x4672F0 (kind 0xF state 26): the window (0x14, 0x12, 0x118, 0x13, 0),
// the toggles at (0x58, 0x28), +9 - 1, the list at (0x27, 60 * +9 + 0x40) - the
// y built on the record pointer's upper half (movzx ax), which only its low 16
// bits carry on; at +9 = 0 a kind-0x1A record in state 0x10, sub-state the
// record's +0x3E, taken unchecked from Effect_FindFree, and +1 up.
extern "C" void __cdecl EffectKind0F_ListOpen() {
    SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
    SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const U y = ((ScKey() & 0xFFFF0000u) | Sc()[9]) * 60u + 0x40u;
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, static_cast<int>(y));
    if (Sc()[9] != 0) return;
    const unsigned char k = SH_CALL(Effect_FindFree)();
    s = Sc();
    unsigned char* const r = SpawnMark(k, "EffectKind0F_ListOpen");
    r[2] = s[0x3E];
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4673B0 (kind 0xF state 27): the window, the toggles and the list at
// rest (0x27, 0x40).
extern "C" void __cdecl EffectKind0F_ListShow() {
    SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
    SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, 0x40);
}

// original 0x467400 (kind 0xF state 28): +9 + 1. With +0xB 0 the window rises
// (y 0x12 - 16 * +9, on the pointer's upper half as in state 26) and the
// toggles slide right (x 45 * +9 + 0x58 over a callee's eax: its low 16 bits),
// all unlit; else both at rest and lit. The list slides down (60 * +9 + 0x40,
// low 16 bits). At +9 = 4: +1 = 0 with +0xB 0, else 2 for +6 = 0, 6 for +6 = 1.
extern "C" void __cdecl EffectKind0F_ListClose() {
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Sc();
    if (s[0xB] == 0) {
        const U y = 0x12u - (((ScKey() & 0xFFFF0000u) | s[9]) << 4);
        SH_CALL(Panel_DrawWindow)(0x14, static_cast<int>(y), 0x118, 0x13, 0);
        SH_CALL(EffectKind0F_DrawToggles)(static_cast<int>(Sc()[9] * 45u + 0x58u), 0x28, 0);
    } else {
        SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
        SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    }
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, static_cast<int>(Sc()[9] * 60u + 0x40u));
    s = Sc();
    if (s[9] != 4) return;
    if (s[0xB] == 0) s[1] = 0;
    else if (s[6] == 0) s[1] = 2;
    else if (s[6] == 1) s[1] = 6;
}

// original 0x4674F0 (kind 0xF state 29): while record 6's +8 is 0 and its +1 is
// 0xE, EffectKind0F_Children[+2] (a tail jump); else Effect_Release.
extern "C" void __cdecl EffectKind0F_Child() {
    if (B(at::kRecord6Hold) == 0 && B(at::kRecord6State) == 0xE) {
        StepAt(at::kChildren, Sc()[2], at::kChildrenCount, "EffectKind0F_Child")();
        return;
    }
    SH_CALL(Effect_Release)();
}

// originals 0x467520, 0x4677F0, 0x467A60, 0x467D60, 0x468020, 0x468320 (kind 0xF
// states 30..35, EffectKind0F_Children 0..5): each sub-kind's step table by +3.
extern "C" void __cdecl EffectKind0F_Child0() {
    StepAt(at::kChild0Steps, Sc()[3], at::kChild0Count, "EffectKind0F_Child0")();
}
extern "C" void __cdecl EffectKind0F_Child1() {
    StepAt(at::kChild1Steps, Sc()[3], at::kChild1Count, "EffectKind0F_Child1")();
}
extern "C" void __cdecl EffectKind0F_Child2() {
    StepAt(at::kChild2Steps, Sc()[3], at::kChild2Count, "EffectKind0F_Child2")();
}
extern "C" void __cdecl EffectKind0F_Child3() {
    StepAt(at::kChild3Steps, Sc()[3], at::kChild3Count, "EffectKind0F_Child3")();
}
extern "C" void __cdecl EffectKind0F_Child4() {
    StepAt(at::kChild4Steps, Sc()[3], at::kChild4Count, "EffectKind0F_Child4")();
}
extern "C" void __cdecl EffectKind0F_Child5() {
    StepAt(at::kChild5Steps, Sc()[3], at::kChild5Count, "EffectKind0F_Child5")();
}

// --- sub-kind 0 (EffectKind0F_Child0Steps) --------------------------------------------

// original 0x467540: two kind-0x1A records in state 0x10, +3 = 2 and 4 (each
// only if Effect_FindFree found one); bank 0x47, placed at (0x5E, 0xBA), +9 =
// +0xA = 0; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child0Start() {
    unsigned char k = SH_CALL(Effect_FindFree)();
    if (k != 0xFF) SpawnMark(k, "EffectKind0F_Child0Start")[3] = 2;
    k = SH_CALL(Effect_FindFree)();
    if (k != 0xFF) SpawnMark(k, "EffectKind0F_Child0Start")[3] = 4;
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0x5E, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467630: +9 0: the next record of the script 0x653CCC (+0xA; an
// animation of 0xFF goes to record `frames` first): Sprite_SetAnimation, +9 its
// frames, +0xA + 1; else +9 - 1 and Sprite_ScriptTick. Then Sprite_QueueOverlay.
extern "C" void __cdecl EffectKind0F_Child0Animate() {
    static const char kWho[] = "EffectKind0F_Child0Animate";
    auto at_ = [](int i) { return ByteOf(at::kChild0Script, at::kChild0ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(2 * s[0xA]) == 0xFF) s[0xA] = at_(2 * s[0xA] + 1);
        SH_CALL(Sprite_SetAnimation)(at_(2 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(2 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x4676A0: bank 0x21, placed at (0xA2, 0xBA) with +0x2A 1; +6, +9,
// +0xA 0, dword +0xC 0, dword +0x18 4; Sprite_SetAnimation(0); +3 + 1.
extern "C" void __cdecl EffectKind0F_Child0Second() {
    SH_CALL(Sprite_SetAnimationBank)(0x21);
    Place(1, 1, 0xA2, 0xBA);
    unsigned char* s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    SetLong(s + 0xC, 0);
    SetLong(s + 0x18, 4);
    SH_CALL(Sprite_SetAnimation)(0);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467750: Sprite_ScriptTick, then Sprite_QueueOverlay (a tail jump).
extern "C" void __cdecl EffectKind0F_Child0Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x467760: bank 0x47, placed at (0xE8, 0xBA), +9 = +0xA = 0,
// Sprite_SetAnimation(0xA), +3 - 1.
extern "C" void __cdecl EffectKind0F_Child0Back() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xE8, 0xBA);
    unsigned char* s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    SH_CALL(Sprite_SetAnimation)(0xA);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] - 1);
}

// --- sub-kind 1 --------------------------------------------------------------------

// original 0x467810: bank 0x47, placed at (0xEC, 0xA2); +6, +9, +0xA 0, dword
// +0xC 0, dword +0x18 4; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child1Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xEC, 0xA2);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    SetLong(s + 0xC, 0);
    SetLong(s + 0x18, 4);
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x4678C0: the script 0x653CEC of (animation, frames, mode) as
// sub-kind 0's (an animation of 0xFF goes to record `frames` and zeroes +0xC,
// +6 and sets +0x18 4); Sprite_QueueOverlay; the draw mode (0, 1) and sprites
// 0x15, 0x16, 0x10. The current record's mode (the one before +0xA): 0 bounces
// dword +0xC by +0x18 between 0 and 0x60 (the step negated at each end); 1
// counts +6 up on even frames. Then 0x465120(0x39, 0x97, word +0xC, 1), +6
// printed at (0xAA, 0x94) and Sprite_QueueOverlay again.
extern "C" void __cdecl EffectKind0F_Child1Meter() {
    static const char kWho[] = "EffectKind0F_Child1Meter";
    auto at_ = [](int i) { return ByteOf(at::kChild1Script, at::kChild1ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(3 * s[0xA]) == 0xFF) {
            s[0xA] = at_(3 * s[0xA] + 1);
            SetLong(s + 0xC, 0);
            SetLong(s + 0x18, 4);
            s[6] = 0;
        }
        SH_CALL(Sprite_SetAnimation)(at_(3 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(3 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
    DrawMode(0, 1);
    DrawSprite(0x15, 1, 0x32, 0x7A);
    DrawSprite(0x16, 1, 0xBC, 0x7A);
    DrawSprite(0x10, 1, 0x36, 0x80);
    s = Sc();
    const unsigned char mode = at_(3 * s[0xA] - 1);
    if (mode == 0) {
        SetLong(s + 0xC, static_cast<std::int32_t>(static_cast<U>(Long(s + 0xC)) + static_cast<U>(Long(s + 0x18))));
        if (Long(s + 0x18) > 0) {
            if (Long(s + 0xC) > 0x60) {
                SetLong(s + 0xC, 0x60);
                SetLong(s + 0x18, static_cast<std::int32_t>(0u - static_cast<U>(Long(s + 0x18))));
            }
        } else if (Long(s + 0xC) < 0) {
            SetLong(s + 0xC, 0);
            SetLong(s + 0x18, static_cast<std::int32_t>(0u - static_cast<U>(Long(s + 0x18))));
        }
    } else if (mode == 1 && (Frame_Counter & 1) == 0) {
        s[6] = static_cast<unsigned char>(s[6] + 1);
    }
    Call4(at::kE1aBarG4, 0x39, 0x97, Word(Sc() + 0xC), 1);
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat), static_cast<unsigned>(Sc()[6]));
    SH_CALL(Text_DrawFont12)(0xAA, 0x94, 0, Text(at::kPrint));
    SH_CALL(Sprite_QueueOverlay)();
}

// --- sub-kind 2 --------------------------------------------------------------------

// original 0x467A80: bank 0x47, placed at (0xE8, 0xBA), +6 = +9 = +0xA = 0; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child2Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xE8, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467B10: the script 0x653D08 of (animation, frames, step) - an
// animation of 0 goes to record `frames` and zeroes +6; a record whose step is
// 0x50 spawns a kind-0x1A record in state 0x10, +2 = +3 = 2, placed at (0xE8,
// 0x9E), from Effect_FindFree unchecked. Sprite_QueueOverlay. Every fourth frame
// the current step (not 0x50) is added to the gauge +6, held to 0..0x18 (a sum
// with bit 7 set is 0). E1A's gauge draws, the draw mode (0, 1), sprite 0x14,
// and with a step the mark at (0xF4, +6 + 0x68 on the pointer's upper half).
extern "C" void __cdecl EffectKind0F_Child2Gauge() {
    static const char kWho[] = "EffectKind0F_Child2Gauge";
    auto at_ = [](int i) { return ByteOf(at::kChild2Script, at::kChild2ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(3 * s[0xA]) == 0) {
            s[0xA] = at_(3 * s[0xA] + 1);
            s[6] = 0;
        }
        SH_CALL(Sprite_SetAnimation)(at_(3 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(3 * s[0xA] + 1);
        s = Sc();
        if (at_(3 * s[0xA] + 2) == 0x50) {
            const unsigned char k = SH_CALL(Effect_FindFree)();
            unsigned char* const r = SpawnMark(k, kWho);
            r[2] = 2;
            r[3] = 2;
            s = Sc();
            SetWord(r + 0x2E, 0xE8);
            SetWord(r + 0x30, 0x9E);
        }
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
    s = Sc();
    const unsigned char step = at_(3 * s[0xA] - 1);
    if (step != 0x50 && (Frame_Counter & 3) == 0) {
        const auto v = static_cast<unsigned char>(s[6] + step);
        s[6] = (v & 0x80) != 0 ? 0 : (v > 0x18 ? 0x18 : v);
    }
    Call3(at::kE1aGaugeA, 0xE4, 0x68, 1);
    Call3(at::kE1aGaugeB, 0xE4, 0x78, 1);
    DrawMode(0, 1);
    DrawSprite(0x14, 1, 0xDC, 0x60);
    s = Sc();
    if (at_(3 * s[0xA] - 1) != 0) Call3(at::kE1aGaugeMark, 0xF4, ((ScKey() & 0xFFFF0000u) | s[6]) + 0x68u, 1);
}

// original 0x467CA0: bank 0x2B; +0x24 0x80, +0x29 = +0x2A = 0, +0x48 2, dwords
// +0x40 = +0x44 = 0x8000, the tint bytes 0, +9 0x40, +6 0;
// Sprite_SetAnimation(3); +3 + 1.
extern "C" void __cdecl EffectKind0F_Child2Fade() {
    SH_CALL(Sprite_SetAnimationBank)(0x2B);
    unsigned char* s = Sc();
    s[0x24] = 0x80;
    s[0x29] = 0;
    s[0x2A] = 0;
    s[0x48] = 2;
    SetLong(s + 0x40, 0x8000);
    SetLong(s + 0x44, 0x8000);
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
    s[9] = 0x40;
    s[6] = 0;
    SH_CALL(Sprite_SetAnimation)(3);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467D30: +9 - 1; at 0 Effect_Release, else with bit 3 of +9
// Sprite_QueueOverlay (both tail jumps).
extern "C" void __cdecl EffectKind0F_Child2Blink() {
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const unsigned char n = Sc()[9];
    if (n == 0) SH_CALL(Effect_Release)();
    else if ((n & 8) != 0) SH_CALL(Sprite_QueueOverlay)();
}

// --- sub-kind 3 --------------------------------------------------------------------

// original 0x467D80: bank 0x47, placed at (0xCA, 0xBA), +9 = +0xA = +0xB = 0;
// +3 + 1, then Sprite_SetAnimation(3).
extern "C" void __cdecl EffectKind0F_Child3Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xCA, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[0xB] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    SH_CALL(Sprite_SetAnimation)(3);
}

// original 0x467E10: the grid 0x653D34 (four rows of marks, each to an 0xFF).
// While +0xA counts down: E1A's 0x4652D0(0xD4, 0x8A, 0, row + 1 over the
// pointer's upper bytes); else +9 + 1 and the lit column is +9 / 12 - at its
// row's 0xFF +0xA = 0x32. With +0xA 0 and the lit mark 1, the sprite script
// ticks for 10 frames of 12. Every mark drawn: the lit one (the row +0xB, the
// column +9 / 12, 10 frames of 12) by Text_DrawAt with the string of index 2 *
// mark, the rest by Text_DrawSmall (the row's colour 3 * mark over the string
// pointer's upper half, the others' 7 - mark). Sprite_QueueOverlay; +0xA down,
// at 0 +9 = 0 and the next row (after 3, row 0).
extern "C" void __cdecl EffectKind0F_Child3Grid() {
    static const char kWho[] = "EffectKind0F_Child3Grid";
    auto grid = [](int i) { return ByteOf(at::kChild3Grid, at::kChild3GridBytes, i, kWho); };
    unsigned char* s = Sc();
    unsigned column;
    if (s[0xA] != 0) {
        Call4(at::kE1aDigits, 0xD4, 0x8A, 0, (ScKey() & 0xFFFFFF00u) | static_cast<unsigned char>(s[0xB] + 1));
        s = Sc();
        column = s[9] / 12u;
    } else {
        s[9] = static_cast<unsigned char>(s[9] + 1);
        s = Sc();
        column = s[9] / 12u;
        if (grid(s[0xB] * 8 + static_cast<int>(column)) == 0xFF) s[0xA] = 0x32;
    }
    if (s[0xA] == 0 && grid(s[0xB] * 8 + static_cast<int>(column)) == 1 && s[9] % 12u < 10) {
        SH_CALL(Sprite_ScriptTick)();
    }
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 8; ++col) {
            const unsigned char mark = grid(static_cast<int>(row * 8 + col));
            if (mark == 0xFF) break;
            s = Sc();
            if (row == s[0xB]) {
                if (s[9] % 12u < 10 && col == column) {
                    const U text = static_cast<U>(Long(At(at::kGridText + 8u * mark)));
                    DrawAt(static_cast<int>(12 * col + 0x84), static_cast<int>(13 * row + 0x9B),
                           static_cast<unsigned char>(mark << 1), 1, Text(text));
                } else {
                    const U text = static_cast<U>(Long(At(at::kGridText + 4u * mark)));
                    const auto colour = static_cast<std::uint16_t>(static_cast<signed char>(mark) * 3);
                    SH_CALL(Text_DrawSmall)(static_cast<int>(12 * col + 0x86), static_cast<int>(13 * row + 0x9E),
                                            (text & 0xFFFF0000u) | colour, 1, Text(text));
                }
            } else {
                const U text = static_cast<U>(Long(At(at::kGridText + 4u * mark)));
                SH_CALL(Text_DrawSmall)(static_cast<int>(12 * col + 0x86), static_cast<int>(13 * row + 0x9E),
                                        static_cast<unsigned char>(7 - mark), 1, Text(text));
            }
        }
    }
    SH_CALL(Sprite_QueueOverlay)();
    s = Sc();
    if (s[0xA] == 0) return;
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    s = Sc();
    if (s[0xA] != 0) return;
    s[9] = 0;
    s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    s = Sc();
    if (s[0xB] > 3) s[0xB] = 0;
}

// --- sub-kinds 4 and 5 -------------------------------------------------------------

// original 0x468040: bank 0x47, placed at (0xF0, 0xBA); +6 = +9 = +0xA = 0,
// dwords +0xC = +0x10 = 0x6A, +0x4B 0xFF; +3 + 1. Also called by
// EffectKind0F_Child5Start.
extern "C" void __cdecl EffectKind0F_Child4Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xF0, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    SetLong(s + 0xC, 0x6A);
    SetLong(s + 0x10, 0x6A);
    s[0x4B] = 0xFF;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

namespace {
// The move scripts of sub-kinds 4 and 5 (0x653D5C, 0x653DAC): records of
// (animation | 0x80 no script tick, frames, dx, dy); an animation of 0xFF goes
// to record `frames`. The step's record: Sprite_EnsureAnimation(animation &
// 0x7F), +9 its frames, +0xA + 1 - returned so sub-kind 5 can test it - or +9 -
// 1 and a tick unless the current record (the one before +0xA) says not; then
// the cursor (dwords +0xC, +0x10) moved by its dx, dy and +6 less dx.
signed char MoveByte(U script, int index, const char* who) {
    return static_cast<signed char>(ByteOf(script, at::kMoveScriptBytes, index, who));
}
void MoveCursor(U script, const char* who) {
    unsigned char* s = Sc();
    SetLong(s + 0xC, static_cast<std::int32_t>(static_cast<U>(Long(s + 0xC)) +
                                               static_cast<U>(static_cast<std::int32_t>(MoveByte(script, 4 * s[0xA] - 2, who)))));
    s = Sc();
    SetLong(s + 0x10, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x10)) +
                                                static_cast<U>(static_cast<std::int32_t>(MoveByte(script, 4 * s[0xA] - 1, who)))));
    s = Sc();
    s[6] = static_cast<unsigned char>(s[6] - static_cast<unsigned char>(MoveByte(script, 4 * s[0xA] - 2, who)));
}
}  // namespace

// original 0x4680F0: the move script 0x653D5C stepped and the cursor moved (as
// above); EffectKind0F_DrawCursorPanel(dy not negative); E1A's
// 0x465D90(0x7E, 0x5C, 0, 1) and 0x465E50(0x8F, 0x69, 0x20, 0, 1, 0).
extern "C" void __cdecl EffectKind0F_Child4Move() {
    static const char kWho[] = "EffectKind0F_Child4Move";
    auto at_ = [](int i) { return ByteOf(at::kChild4Script, at::kMoveScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(4 * s[0xA]) == 0xFF) s[0xA] = at_(4 * s[0xA] + 1);
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(at_(4 * Sc()[0xA]) & 0x7F));
        s = Sc();
        s[9] = at_(4 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        s = Sc();
        if ((at_(4 * s[0xA] - 4) & 0x80) == 0) SH_CALL(Sprite_ScriptTick)();
    }
    MoveCursor(at::kChild4Script, kWho);
    SH_CALL(EffectKind0F_DrawCursorPanel)(MoveByte(at::kChild4Script, 4 * Sc()[0xA] - 1, kWho) >= 0 ? 1u : 0u);
    Call4(at::kE1aMarker, 0x7E, 0x5C, 0, 1);
    Call6(at::kE1aRange, 0x8F, 0x69, 0x20, 0, 1, 0);
}

// original 0x468210 (down, a byte): Sprite_QueueOverlay; the draw mode (0, 1),
// sprites 0x15, 0x16, 0x11; the cursor held to x 0x39..0x79 and y 0x41..0x94;
// E1A's range 0x465E50(x, 0xB3, 0x20, (+6 & 7) * 2, 1, y outside x..x + 0x20)
// and marker 0x465D90(y, 0xB3, down 0 ? 0 : 1, 1).
extern "C" void __cdecl EffectKind0F_DrawCursorPanel(unsigned down) {
    SH_CALL(Sprite_QueueOverlay)();
    DrawMode(0, 1);
    DrawSprite(0x15, 1, 0x32, 0x96);
    DrawSprite(0x16, 1, 0xBC, 0x96);
    DrawSprite(0x11, 1, 0x36, 0x9C);
    unsigned char* s = Sc();
    std::int32_t v = Long(s + 0xC);
    SetLong(s + 0xC, v < 0x39 ? 0x39 : (v > 0x79 ? 0x79 : v));
    s = Sc();
    v = Long(s + 0x10);
    SetLong(s + 0x10, v < 0x41 ? 0x41 : (v > 0x94 ? 0x94 : v));
    s = Sc();
    const std::int32_t cx = Long(s + 0xC), cy = Long(s + 0x10);
    const U outside = (cy >= cx && cy <= cx + 0x20) ? 0u : 1u;
    Call6(at::kE1aRange, Word(s + 0xC), 0xB3, 0x20, static_cast<unsigned char>((s[6] & 7) << 1), 1, outside);
    Call4(at::kE1aMarker, Word(Sc() + 0x10), 0xB3, (down & 0xFF) == 0 ? 0u : 1u, 1);
}

// original 0x468340: EffectKind0F_Child4Start, then +0xB = 0x37.
extern "C" void __cdecl EffectKind0F_Child5Start() {
    SH_CALL(EffectKind0F_Child4Start)();
    Sc()[0xB] = 0x37;
}

// original 0x468350: the move script 0x653DAC as sub-kind 4's (going back to
// record `frames` also sets +0xB 0x37 and the cursor to (0x6A, 0x6A)); a record
// of animation 9 spawns a kind-0x1A record in state 0x10, +2 = +3 = 2, at
// (0xF2, 0x9E), when Effect_FindFree finds one. The cursor moved; the panel
// drawn; every 16th frame a positive dy counts +0xB up to 0x3C; +0xB printed at
// (0xA9, 0xB0) in colour 2 on frames with bit 2, else 0.
extern "C" void __cdecl EffectKind0F_Child5Move() {
    static const char kWho[] = "EffectKind0F_Child5Move";
    auto at_ = [](int i) { return ByteOf(at::kChild5Script, at::kMoveScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(4 * s[0xA]) == 0xFF) {
            s[0xA] = at_(4 * s[0xA] + 1);
            s[0xB] = 0x37;
            SetLong(s + 0xC, 0x6A);
            SetLong(s + 0x10, 0x6A);
        }
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(at_(4 * Sc()[0xA]) & 0x7F));
        s = Sc();
        if ((at_(4 * s[0xA]) & 0x7F) == 9) {
            const unsigned char k = SH_CALL(Effect_FindFree)();
            if (k != 0xFF) {
                unsigned char* const r = SpawnMark(k, kWho);
                r[2] = 2;
                r[3] = 2;
                SetWord(r + 0x2E, 0xF2);
                SetWord(r + 0x30, 0x9E);
            }
        }
        s = Sc();
        s[9] = at_(4 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        s = Sc();
        if ((at_(4 * s[0xA] - 4) & 0x80) == 0) SH_CALL(Sprite_ScriptTick)();
    }
    MoveCursor(at::kChild5Script, kWho);
    SH_CALL(EffectKind0F_DrawCursorPanel)(MoveByte(at::kChild5Script, 4 * Sc()[0xA] - 1, kWho) >= 0 ? 1u : 0u);
    if ((Frame_Counter & 0xF) == 0) {
        s = Sc();
        if (MoveByte(at::kChild5Script, 4 * s[0xA] - 1, kWho) > 0) {
            s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
            if (s[0xB] > 0x3C) s[0xB] = 0x3C;
        }
    }
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat), static_cast<unsigned>(Sc()[0xB]));
    SH_CALL(Text_DrawFont12)(0xA9, 0xB0, (Frame_Counter & 4) != 0 ? 2 : 0, Text(at::kPrint));
}

// --- the panels ---------------------------------------------------------------------

// original 0x468560 (x, y): the window (x, y, 0xF2, 0x9F); with +8 (the scroll's
// direction, signed) not 0 the list scrolls: at +9 0 a new title (+0x3C) sets
// the target +0x20 = 9 * +0x3C, a step down for +8 > 0 (+0x14 wraps below 0 to
// 0x35), +9 and +0xA up; then +0xA 0: at +0x14 = +0x20 the scroll ends (+8 =
// +9 = 0, a kind-0x1A record in state 0x10, sub-state +0x3E, from
// Effect_FindFree unchecked), else another step down and +0xA up; +0xA not 0:
// +0xA up, at 4 back to 0 with a step up for +8 < 0 (past 0x35 to 0). Nine or
// ten lines of the script pool from +0x14 (the line table 0x653E00, ids 0xFFFF
// skipped), offset by 3 * +0xA; the two bars, the title, the frame and
// "+0x3C + 1" printed.
extern "C" void __cdecl EffectKind0F_DrawMessageList(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawMessageList";
    SH_CALL(Menu_DrawBox)(x, y, 0xF2, 0x9F, 0xF0, Style());
    unsigned char* s = Sc();
    auto step_down = [](unsigned char* r) {
        if (static_cast<signed char>(r[8]) <= 0) return r;
        SetLong(r + 0x14, Long(r + 0x14) - 1);
        r = Sc();
        if (Long(r + 0x14) < 0) {
            SetLong(r + 0x14, 0x35);
            r = Sc();
        }
        return r;
    };
    if (s[8] != 0) {
        if (s[9] == 0) {
            if (static_cast<int>(static_cast<short>(Word(s + 0x3E))) != static_cast<int>(Word(s + 0x3C))) {
                SetLong(s + 0x20, static_cast<std::int32_t>(Word(s + 0x3C) * 9u));
                s = Sc();
                SetWord(s + 0x3E, Word(s + 0x3C));
                s = Sc();
            }
            s = step_down(s);
            s[9] = static_cast<unsigned char>(s[9] + 1);
            s = Sc();
            s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            s = Sc();
        }
        if (s[0xA] == 0) {
            if (Long(s + 0x14) == Long(s + 0x20)) {
                s[9] = 0;
                Sc()[8] = 0;
                const unsigned char k = SH_CALL(Effect_FindFree)();
                s = Sc();
                SpawnMark(k, kWho)[2] = s[0x3E];
            } else {
                s = step_down(s);
                s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            }
        } else {
            s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            s = Sc();
            if (s[0xA] == 4) {
                s[0xA] = 0;
                s = Sc();
                if (static_cast<signed char>(s[8]) < 0) {
                    SetLong(s + 0x14, Long(s + 0x14) + 1);
                    s = Sc();
                    if (Long(s + 0x14) > 0x35) SetLong(s + 0x14, 0);
                }
            }
        }
    }
    s = Sc();
    U top;
    unsigned lines;
    if (s[0xA] != 0) {
        top = s[8] == 1 ? static_cast<U>(y) + 3u * (s[0xA] + 4u) : static_cast<U>(y) - 3u * s[0xA] + 0x19u;
        lines = 10;
    } else {
        top = static_cast<U>(y) + 0x19u;
        lines = 9;
    }
    auto line = static_cast<unsigned char>(Long(s + 0x14));
    for (unsigned n = 0; n < lines; ++n) {
        const std::uint16_t id = WordOf(at::kLineIds, at::kLineCount, line, kWho);
        if (id != 0xFFFF) DrawAt(x + 6, static_cast<int>(top + 13 * n), 0, 0xFF, Pool(id));
        line = static_cast<unsigned char>(line + 1);
        if (line > 0x35) line = 0;
    }
    SH_CALL(Menu_DrawBox)(x, y + 6, 0xF2, 0xE, 0, Style());
    SH_CALL(Menu_DrawBox)(x, y + 0x94, 0xF2, 0xB, 0x30, Style());
    DrawAt(x + 0x3D, y + 6, 0, 0xFF, Pool(WordOf(at::kTitleIds, at::kTitleCount, Word(Sc() + 0x3C), kWho)));
    SH_CALL(EffectKind0F_DrawListFrame)(x, y);
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Text(at::kPageFormat)), Word(Sc() + 0x3C) + 1u);
    SH_CALL(Text_DrawFont8)(x + 0xCA, y + 0x92, 0, Text(at::kPrint));
}

// original 0x468840 (x, y): the draw mode (9, 1); the top edge (sprite 0x25, 23
// of 0x26 every 8 from x + 0x20, 0x27, 0x28), the two side quads (0x78 high,
// edges 0 and 1), the bottom edge (0x2A, 23 of 0x2B from x + 8, 0x2C, three
// 0x2D, 0x2E, 0x2F) at y + 0x90.
extern "C" void __cdecl EffectKind0F_DrawListFrame(int x, int y) {
    DrawMode(9, 1);
    DrawSprite(0x25, 1, x, y);
    for (int i = 0; i < 0x17; ++i) DrawSprite(0x26, 1, x + 8 * i + 0x20, y);
    DrawSprite(0x27, 1, x + 0xD8, y);
    DrawSprite(0x28, 1, x + 0xF0, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x78, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0xF0, y + 0x18, 0x78, 1);
    DrawSprite(0x2A, 1, x, y + 0x90);
    for (int i = 0; i < 0x17; ++i) DrawSprite(0x2B, 1, x + 8 * i + 8, y + 0x90);
    DrawSprite(0x2C, 1, x + 0xC0, y + 0x90);
    for (int i = 0; i < 3; ++i) DrawSprite(0x2D, 1, x + 8 * i + 0xC8, y + 0x90);
    DrawSprite(0x2E, 1, x + 0xE0, y + 0x90);
    DrawSprite(0x2F, 1, x + 0xE7, y + 0x90);
}

// original 0x468950 (x, y, height, which): a POLY_FT4 at the packet cursor (the
// pointer read once, before Gpu_SetPolyFT4, as the original keeps it): tpage
// word 0x7887, the CLUT word ((c & 0x3C0) | 0x400) >> 6 of the record's c; the
// corners (x, y), (x + w, y), (x, y + height), (x + w, y + height) as floats (x,
// y s16, height a byte, w the record's word); u, u + w, v, v + h from the
// record's bytes; shade 0x80; committed (1, 0x48). The records 0x653E6C, five of
// ten bytes, by the low byte of `which`.
extern "C" void __cdecl Panel_DrawEdgeQuad(int x, int y, unsigned height, unsigned which) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const unsigned k = which & 0xFF;
    if (k >= at::kEdgeQuadCount)
        bof3::Fatal("Panel_DrawEdgeQuad: quad %u past the %u records of 0x%X (the original reads past them)", k,
                    at::kEdgeQuadCount, (unsigned)at::kEdgeQuads);
    const unsigned char* const r = At(at::kEdgeQuads + 10 * k);
    SetWord(p + 0x16, 0x7887);
    SetWord(p + 0x26, ((Word(r + 8) & 0x3C0u) | 0x400u) >> 6);
    const int sx = static_cast<short>(x), sy = static_cast<short>(y);
    F(p + 8, sx);
    F(p + 0x18, static_cast<int>(Word(r + 4)) + sx);
    F(p + 0x28, sx);
    F(p + 0x38, static_cast<int>(Word(r + 4)) + sx);
    F(p + 0xC, sy);
    F(p + 0x1C, sy);
    F(p + 0x2C, sy + static_cast<int>(height & 0xFF));
    F(p + 0x3C, sy + static_cast<int>(height & 0xFF));
    const auto u2 = static_cast<unsigned char>(r[0] + r[4]);
    p[0x14] = r[0];
    p[0x24] = u2;
    p[0x34] = p[0x14];
    p[0x44] = u2;
    p[0x15] = r[2];
    p[0x25] = r[2];
    p[0x35] = p[0x45] = static_cast<unsigned char>(r[6] + r[2]);
    p[4] = p[5] = p[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x468A40: the draw mode (0, 1), sprite 0x12 at (0xF0, 0x42), the
// label 0x653EBC at (0x119, 0x45); s16 +0x3E + 1 printed and drawn at (0xF7,
// 0x45), two characters. (A first character of 0x20 is written back as 0x20:
// the original's store changes nothing.)
extern "C" void __cdecl EffectKind0F_DrawCountHeader() {
    DrawMode(0, 1);
    DrawSprite(0x12, 1, 0xF0, 0x42);
    SH_CALL(Text_DrawFont12)(0x119, 0x45, 0, Text(at::kCountLabel));
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat),
                         static_cast<int>(static_cast<short>(Word(Sc() + 0x3E))) + 1);
    if (B(at::kPrint) == 0x20) B(at::kPrint) = 0x20;
    DrawAt(0xF7, 0x45, 0, 2, Text(at::kPrint));
}

// original 0x468AC0 (x, y, bits): three toggle boxes at x + 0x30 i, each lit by
// bit i of the byte; the three labels (the pointers 0x66A088..0x66A090) at x +
// 0xA, 0x35, 0x6A, y + 4, two, three and two characters: colour 0 when the byte
// is 0, else 0 for a lit box and 7 for the others. (The original keeps the
// three bits in its first argument's slot.)
extern "C" void __cdecl EffectKind0F_DrawToggles(int x, int y, unsigned bits) {
    const auto byte = static_cast<unsigned char>(bits);
    unsigned char lit[3];
    for (int i = 0; i < 3; ++i) {
        lit[i] = static_cast<unsigned char>((byte >> i) & 1);
        SH_CALL(EffectKind0F_DrawToggle)(x + 0x30 * i, y, lit[i]);
    }
    static const int kX[3] = {0xA, 0x35, 0x6A}, kCount[3] = {2, 3, 2};
    for (int i = 0; i < 3; ++i) {
        const auto text = static_cast<U>(Long(At(at::kToggleText + 4u * i)));
        const int colour = byte == 0 ? 0 : static_cast<unsigned char>(7 - 7 * lit[i]);
        if (FishingText_On()) {
            // DIVERGENCE DIV-0069: under a Latin overlay the label whole, centred
            // in its box (x + 0x30 i + 2, 0x28 wide) by its real width - the US
            // module's x + 6 + 0x30 i for its four letters.
            const int width = static_cast<int>(TextAdvance_Width(Text(text)));
            DrawAt(x + 0x30 * i + 0x16 - width / 2, y + 4, colour, 0xFF, Text(text));
        } else {
            DrawAt(x + kX[i], y + 4, colour, kCount[i], Text(text));
        }
    }
}

// original 0x468BB0 (x, y, lit): a box (x + 2, y + 2, 0x28, 0x10); the draw mode
// (8, 1); sprites 4 * lit + 0x1D at (x, y), + 0x1E at (x, y + 0x10), four of +
// 0x1F from x + 8 and (lit + 8) * 4 at x + 0x28, all byte ids.
extern "C" void __cdecl EffectKind0F_DrawToggle(int x, int y, unsigned lit) {
    SH_CALL(Menu_DrawBox)(x + 2, y + 2, 0x28, 0x10, 0, Style());
    DrawMode(8, 1);
    const auto l4 = static_cast<unsigned char>(lit << 2);
    DrawSprite(static_cast<unsigned char>(l4 + 0x1D), 1, x, y);
    DrawSprite(static_cast<unsigned char>(l4 + 0x1E), 1, x, y + 0x10);
    for (int i = 0; i < 4; ++i) DrawSprite(static_cast<unsigned char>(l4 + 0x1F), 1, x + 8 * i + 8, y + 0x10);
    DrawSprite(static_cast<unsigned char>((lit + 8) << 2), 1, x + 0x28, y + 0x10);
}

// original 0x468C50 (x, y): the window (x, y + 1, 0x8B, 0x9E); every accessory of
// the inventory's 0x80 ids whose category is 0xB, a row of 13 each: the one at
// dword +0xC gives its index to word +0x36 and, with the leader's +4 2 and +7
// bit 0 clear, is drawn lit (its name at +0x19 in colour 7 over +0x17 in 0, a
// count above 1 likewise); the others their name and count in 0. Then +0x18 the
// number drawn and +0xC held below it; the frame, the scroll mark (x + 0x80, y +
// 0x1C, 0x72) and the label 0x653EA0.
extern "C" void __cdecl EffectKind0F_DrawItemsB(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawItemsB";
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x8B, 0x9E, 0xF0, Style());
    unsigned drawn = 0;
    for (unsigned n = 0; n < 0x80; ++n) {
        if (Category(B(at::kItemIds + n), kWho) != 0xB) continue;
        unsigned char* s = Sc();
        if (Long(s + 0xC) == static_cast<std::int32_t>(drawn)) SetWord(s + 0x36, n);
        s = Sc();
        const int row = y + 13 * static_cast<int>(drawn);
        if (B(at::kLeader4) == 2 && (s[7] & 1) == 0 && Long(s + 0xC) == static_cast<std::int32_t>(drawn)) {
            DrawAt(x + 5, row + 0x19, 7, NameCount(), AccessoryName(B(at::kItemIds + n)));
            DrawAt(x + 5, row + 0x17, 0, NameCount(), AccessoryName(B(at::kItemIds + n)));
            const unsigned char count = B(at::kItemCounts + n);
            if (count > 1) {
                SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Text(at::kCountFormat)), static_cast<unsigned>(count));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1D, 7, Text(at::kPrint));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1B, 0, Text(at::kPrint));
            }
        } else {
            DrawAt(x + 5, row + 0x19, 0, NameCount(), AccessoryName(B(at::kItemIds + n)));
            const unsigned char count = B(at::kItemCounts + n);
            if (count > 1) {
                SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Text(at::kCountFormat)), static_cast<unsigned>(count));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1D, 0, Text(at::kPrint));
            }
        }
        ++drawn;
    }
    unsigned char* s = Sc();
    SetLong(s + 0x18, static_cast<std::int32_t>(drawn));
    s = Sc();
    if (Long(s + 0xC) >= Long(s + 0x18)) SetLong(s + 0xC, Long(s + 0x18) == 0 ? 0 : Long(s + 0x18) - 1);
    SH_CALL(EffectKind0F_DrawItemFrame)(x, y);
    SH_CALL(EffectKind0F_DrawScrollMark)(x + 0x80, y + 0x1C, 0x72);
    DrawAt(x + 0x35, y + 7, 0, 3, Text(at::kItemsBLabel));
}

// original 0x468E50 (x, y, height): a POLY_FT4 (the cursor read once, before
// Gpu_SetPolyFT4): tpage 0x7887, CLUT word 0x1D; corners (x, y), (x + 8, y), (x,
// y + height), (x + 8, y + height) as floats (x, y s16, height a byte); u 0x58,
// 0x60, v 0x9A, 0x9E; shade 0x80; committed (1, 0x48).
extern "C" void __cdecl EffectKind0F_DrawScrollMark(int x, int y, unsigned height) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const int sx = static_cast<short>(x), sy = static_cast<short>(y);
    SetWord(p + 0x26, 0x1D);
    SetWord(p + 0x16, 0x7887);
    F(p + 8, sx);
    F(p + 0x18, sx + 8);   // fadd 8.0f (0x5C41CC): a whole number
    F(p + 0x28, sx);
    F(p + 0x38, sx + 8);
    F(p + 0xC, sy);
    F(p + 0x1C, sy);
    F(p + 0x2C, sy + static_cast<int>(height & 0xFF));
    F(p + 0x3C, sy + static_cast<int>(height & 0xFF));
    p[0x24] = p[0x44] = 0x60;
    p[0x14] = p[0x34] = 0x58;
    p[0x15] = p[0x25] = 0x9A;
    p[0x35] = p[0x45] = 0x9E;
    p[4] = p[5] = p[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x468F00 (x, y): the window (x, y + 1, 0x8B, 0x9E, 0x80). With +0xA
// (a scroll's frames) not 0: +0xA - 1, ten rows from +0x38 (+8, the direction,
// 0: from +0x38 - 1), offset 0x19 - 4 * +0xA (+8 not 0) or 4 * +0xA + 0xD; else
// nine from +0x38 at 0x19. Every accessory of category 0xA from that one on, a
// row of 13: dword +0x10's is lit (leader's +4 2 and +7 bit 0 set) and gives its
// index to word +0x3A. +0x1C the category's count less 1; +0x10 held below the
// rows drawn. The frame; the scroll mark at y + 0x1C + 6 * +0x38 (+-2 * +0xA),
// its height 0xA8 - 6 * count (0x72, at 0x1C, for nine or fewer); the label
// 0x653EA4.
extern "C" void __cdecl EffectKind0F_DrawItemsA(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawItemsA";
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x8B, 0x9E, 0x80, Style());
    unsigned char* s = Sc();
    unsigned char top, count, offset;
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = Sc();
        const unsigned char held = s[8];
        top = held != 0 ? s[0x38] : static_cast<unsigned char>(s[0x38] - 1);
        count = 10;
        offset = held != 0 ? static_cast<unsigned char>(0x19 - static_cast<unsigned char>(s[0xA] << 2))
                           : static_cast<unsigned char>((s[0xA] << 2) + 0xD);
    } else {
        top = s[0x38];
        count = 9;
        offset = 0x19;
    }
    unsigned drawn = 0;
    std::int32_t seen = 0;
    for (unsigned n = 0; n < 0x80; ++n) {
        const unsigned char id = B(at::kItemIds + n);
        if (Category(id, kWho) != 0xA) continue;
        if (seen >= static_cast<std::int32_t>(top) && drawn < count) {
            s = Sc();
            const int row = y + 13 * static_cast<int>(drawn) + offset;
            if (B(at::kLeader4) == 2 && (s[7] & 1) != 0 && Long(s + 0x10) == static_cast<std::int32_t>(drawn)) {
                DrawAt(x + 5, row, 7, NameCount(), AccessoryName(id));
                DrawAt(x + 5, row - 2, 0, NameCount(), AccessoryName(B(at::kItemIds + n)));
                const unsigned char c = B(at::kItemCounts + n);
                if (c > 1) {
                    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Text(at::kCountFormat)), static_cast<unsigned>(c));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 4, 7, Text(at::kPrint));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 2, 0, Text(at::kPrint));
                }
            } else {
                DrawAt(x + 5, row, 0, NameCount(), AccessoryName(id));
                const unsigned char c = B(at::kItemCounts + n);
                if (c > 1) {
                    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Text(at::kCountFormat)), static_cast<unsigned>(c));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 4, 0, Text(at::kPrint));
                }
            }
            s = Sc();
            if (Long(s + 0x10) == static_cast<std::int32_t>(drawn)) SetWord(s + 0x3A, n);
            ++drawn;
        }
        ++seen;
    }
    s = Sc();
    SetLong(s + 0x1C, seen - 1);
    s = Sc();
    if (Long(s + 0x10) >= static_cast<std::int32_t>(drawn))
        SetLong(s + 0x10, Long(s + 0x1C) == 0 || drawn == 0 ? 0 : static_cast<std::int32_t>(drawn - 1));
    SH_CALL(EffectKind0F_DrawItemFrame)(x, y);
    int total = 0;
    for (unsigned n = 0; n < 0x80; ++n)
        if (Category(B(at::kItemIds + n), kWho) == 0xA) ++total;
    unsigned char height, mark;
    if (total <= 9) {
        height = 0x72;
        mark = 0x1C;
    } else {
        height = static_cast<unsigned char>(0xA8 - static_cast<unsigned char>(total * 6));
        s = Sc();
        mark = static_cast<unsigned char>(s[0x38] * 6 + 0x1C);
        const unsigned char a = s[0xA];
        if (a != 0) mark = static_cast<unsigned char>(mark + (s[8] != 0 ? a * 2 : a * 0xFE));
    }
    SH_CALL(EffectKind0F_DrawScrollMark)(x + 0x80, y + mark, height);
    DrawAt(x + 0x2F, y + 7, 0, 4, Text(at::kItemsALabel));
}

// original 0x469210 (x, y): the twin frame; the label 0x653EA8 at (x + 0x2A, y
// + 7). By +7: bit 7 - both equipped accessories (0x904130, 0x90412E) with
// their icons (E1G's 0x594D50) and names in colour 0; else bit 0 clear - the
// first lit (icon flag 1, name in 7 over the icon and name again, the last in
// +7's colour), the second plain; bit 0 set - the first plain, the second lit
// (its last name in colour +7 & 2). The label 0x653EAD at (x + 0x2A, y + 0x47);
// unless word +0x2C is 0xFFFF, that script-pool message at (0x19, 0x9A).
extern "C" void __cdecl EffectKind0F_DrawEquipped(int x, int y) {
    SH_CALL(EffectKind0F_DrawTwinFrame)(x, y);
    DrawAt(x + 0x2A, y + 7, 0, 5, Text(at::kEquipLabel));
    const unsigned char bits = Sc()[7];
    const U ix = static_cast<U>(x) + 0x10u;
    auto icon = [ix](int iy, unsigned char item, U flag) { Call5(at::kE1gItemIcon, ix, static_cast<U>(iy), item, 3, flag); };
    if ((bits & 0x80) != 0) {
        icon(y + 0x1C, B(at::kEquipA), 0);
        DrawAt(x + 0x1C, y + 0x1A, 0, NameCount(), AccessoryName(B(at::kEquipA)));
        icon(y + 0x2C, B(at::kEquipB), 0);
        DrawAt(x + 0x1C, y + 0x2A, 0, NameCount(), AccessoryName(B(at::kEquipB)));
    } else if ((bits & 1) == 0) {
        icon(y + 0x1C, B(at::kEquipA), 1);
        DrawAt(x + 0x1C, y + 0x1A, 7, NameCount(), AccessoryName(B(at::kEquipA)));
        icon(y + 0x1A, B(at::kEquipA), 0);
        DrawAt(x + 0x1C, y + 0x18, Sc()[7], NameCount(), AccessoryName(B(at::kEquipA)));
        icon(y + 0x2C, B(at::kEquipB), 0);
        DrawAt(x + 0x1C, y + 0x2A, 0, NameCount(), AccessoryName(B(at::kEquipB)));
    } else {
        icon(y + 0x1C, B(at::kEquipA), 0);
        DrawAt(x + 0x1C, y + 0x1A, 0, NameCount(), AccessoryName(B(at::kEquipA)));
        icon(y + 0x2C, B(at::kEquipB), 1);
        DrawAt(x + 0x1C, y + 0x2A, 7, NameCount(), AccessoryName(B(at::kEquipB)));
        icon(y + 0x2A, B(at::kEquipB), 0);
        DrawAt(x + 0x1C, y + 0x28, Sc()[7] & 2, NameCount(), AccessoryName(B(at::kEquipB)));
    }
    DrawAt(x + 0x2A, y + 0x47, 0, 5, Text(at::kEquipLabel2));
    const std::uint16_t message = Word(Sc() + 0x2C);
    if (message != 0xFFFF) DrawAt(0x19, 0x9A, 0, 0xFF, Pool(message));
}

// original 0x469490 (x, y): windows (x, y + 2, 0x8A, 0x3A) and (x, y + 0x42,
// 0x8A, 0x5C); the draw mode (9, 1); two sprite frames, each a top edge (0x25,
// ten of 0x26 from x + 0x20, 0x27 at x + 0x70, 0x28 at x + 0x88), two side quads
// (0x1E high, then 0x40) and a bottom edge (0x3C, sixteen of 0x3D from x + 8,
// 0x3E at x + 0x88), at y and y + 0x36, then y + 0x40 and y + 0x98.
extern "C" void __cdecl EffectKind0F_DrawTwinFrame(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 2, 0x8A, 0x3A, 0x80, Style());
    SH_CALL(Menu_DrawBox)(x, y + 0x42, 0x8A, 0x5C, 0x80, Style());
    DrawMode(9, 1);
    DrawSprite(0x25, 1, x, y);
    for (int i = 0; i < 10; ++i) DrawSprite(0x26, 1, x + 8 * i + 0x20, y);
    DrawSprite(0x27, 1, x + 0x70, y);
    DrawSprite(0x28, 1, x + 0x88, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x1E, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x88, y + 0x18, 0x1E, 1);
    DrawSprite(0x3C, 1, x, y + 0x36);
    for (int i = 0; i < 16; ++i) DrawSprite(0x3D, 1, x + 8 * i + 8, y + 0x36);
    DrawSprite(0x3E, 1, x + 0x88, y + 0x36);
    DrawSprite(0x25, 1, x, y + 0x40);
    for (int i = 0; i < 10; ++i) DrawSprite(0x26, 1, x + 8 * i + 0x20, y + 0x40);
    DrawSprite(0x27, 1, x + 0x70, y + 0x40);
    DrawSprite(0x28, 1, x + 0x88, y + 0x40);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x58, 0x40, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x88, y + 0x58, 0x40, 1);
    DrawSprite(0x3C, 1, x, y + 0x98);
    for (int i = 0; i < 16; ++i) DrawSprite(0x3D, 1, x + 8 * i + 8, y + 0x98);
    DrawSprite(0x3E, 1, x + 0x88, y + 0x98);
}

// original 0x469630 (x, y): the window (x, y + 2, 0x8B, 0x14); the draw mode (9,
// 1); the top edge (0x3F, 0x40, 0x41, eight of 0x26 from x + 0x28, 0x42, 0x43,
// 0x44), a side quad (0x78 high, edge 0), sprite 0x46 at (x + 0x80, y + 0x18),
// a quad (0x68 high, edge 4) at (x + 0x80, y + 0x28), the bottom edge (0x2A,
// fifteen of 0x2B from x + 8) at y + 0x90 and 0x45 at (x + 0x80, y + 0x88).
extern "C" void __cdecl EffectKind0F_DrawItemFrame(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 2, 0x8B, 0x14, 0, Style());
    DrawMode(9, 1);
    DrawSprite(0x3F, 1, x, y);
    DrawSprite(0x40, 1, x + 8, y);
    DrawSprite(0x41, 1, x + 0x10, y);
    for (int i = 0; i < 8; ++i) DrawSprite(0x26, 1, x + 8 * i + 0x28, y);
    DrawSprite(0x42, 1, x + 0x68, y);
    DrawSprite(0x43, 1, x + 0x78, y);
    DrawSprite(0x44, 1, x + 0x80, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x78, 0);
    DrawSprite(0x46, 1, x + 0x80, y + 0x18);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x80, y + 0x28, 0x68, 4);
    DrawSprite(0x2A, 1, x, y + 0x90);
    for (int i = 0; i < 15; ++i) DrawSprite(0x2B, 1, x + 8 * i + 8, y + 0x90);
    DrawSprite(0x45, 1, x + 0x80, y + 0x88);
}

// --- the window (FE2's trade screen and the kinds' panels) --------------------------

// original 0x469750 (x, y, w, h, colour): the bevel (x, y, w, h), then the edges
// (x + 2, y + 2, w - 5, h - 5, colour).
extern "C" void __cdecl Panel_DrawWindow(int x, int y, int w, int h, unsigned colour) {
    SH_CALL(Panel_DrawWindowBevel)(x, y, w, h);
    SH_CALL(Panel_DrawWindowEdges)(x + 2, y + 2, w - 5, h - 5, colour);
}

namespace {
// The window style's colour: the first word of its 0x40 bytes at 0x80B7A8 (the
// style byte signed), as 5:5:5 (r, g, b) bytes shifted up by 3.
struct Colour { unsigned char r, g, b; };
Colour StyleColour() {
    const std::uint16_t c = Word(At(at::kStyleColours + static_cast<U>(64 * static_cast<int>(static_cast<signed char>(Style())))));
    return {static_cast<unsigned char>((c & 0x1F) << 3), static_cast<unsigned char>(((c >> 5) & 0x1F) << 3),
            static_cast<unsigned char>(((c >> 10) & 0x1F) << 3)};
}
void Shade(unsigned char* p, const Colour& c) {
    p[4] = c.r;
    p[5] = c.g;
    p[6] = c.b;
}
}  // namespace

// original 0x469790 (x, y, w, h; each its low 16 bits): the style's colour; the
// draw mode (6, 2); a POLY_F4 (x + 2, y), (x + w - 2, y), (x, y + 2), (x + w, y +
// 2), a TILE (x, y + 2, w, h - 4) and a POLY_F4 (x, y + h - 2), (x + w, y + h -
// 2), (x + 2, y + h), (x + w - 2, y + h), each semi-transparent (1) and
// committed to slot 2 (0x38, 0x1C, 0x38). Each primitive's pointer is read once,
// before its Gpu_Set* call, as the original keeps it.
extern "C" void __cdecl Panel_DrawWindowBevel(int x, int y, int w, int h) {
    const Colour c = StyleColour();
    DrawMode(6, 2);
    const int X = x & 0xFFFF, Y = y & 0xFFFF, W = w & 0xFFFF, H = h & 0xFFFF;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    F(p + 8, X + 2);
    F(p + 0x14, X + W - 2);
    F(p + 0x20, X);
    F(p + 0x2C, X + W);
    F(p + 0xC, Y);
    F(p + 0x18, Y);
    F(p + 0x24, Y + 2);
    F(p + 0x30, Y + 2);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    F(p + 8, X);
    F(p + 0xC, Y + 2);
    F(p + 0x14, W);
    F(p + 0x18, H - 4);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    F(p + 8, X);
    F(p + 0x14, X + W);
    F(p + 0x20, X + 2);
    F(p + 0x2C, X + W - 2);
    F(p + 0xC, Y + H - 2);
    F(p + 0x18, Y + H - 2);
    F(p + 0x24, Y + H);
    F(p + 0x30, Y + H);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
}

// original 0x469960 (x, y, w, h, colour; each of x..h its low 16 bits, colour a
// byte): the style's colour; the draw mode (7 with a colour, else 6; slot 2), a
// LINE_F3 (x, y + h), (x + w, y + h), (x + w, y); the draw mode (6 or 7), a
// LINE_F3 (x, y + h), (x, y), (x + w, y); each semi-transparent and committed (2,
// 0x2C).
extern "C" void __cdecl Panel_DrawWindowEdges(int x, int y, int w, int h, unsigned colour) {
    const Colour c = StyleColour();
    const bool lit = (colour & 0xFF) != 0;
    DrawMode(lit ? 7 : 6, 2);
    const int X = x & 0xFFFF, Y = y & 0xFFFF, W = w & 0xFFFF, H = h & 0xFFFF;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    F(p + 8, X);
    F(p + 0xC, Y + H);
    F(p + 0x14, X + W);
    F(p + 0x18, Y + H);
    F(p + 0x20, X + W);
    F(p + 0x24, Y);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
    DrawMode(lit ? 6 : 7, 2);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    F(p + 8, X);
    F(p + 0xC, Y + H);
    F(p + 0x14, X);
    F(p + 0x18, Y);
    F(p + 0x20, X + W);
    F(p + 0x24, Y);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
}

// original 0x469AD0 (clut, text, width, x): a code-0x6C glyph primitive at the
// cursor (read once, before Gpu_SetCode6C): the glyph of the text's first
// character (two bytes with bit 7: ((b0 & 0x7F) << 8) + b1, else b0 - 0x26) to
// word +0x16; u from 0xC - (width & 0x7F) when width has bit 7, else 0, to u +
// (width & 0x7F); v 0..0xC; x, x + (width & 0x7F) (words); y record 3's dword
// +0x30 + 3, + 0xC more; shade 0x80; CLUT 0x7800 | (clut & 0xF); committed (1,
// 0x28).
extern "C" void __cdecl EffectKind0F_DrawGlyph(unsigned clut, const unsigned char* text, unsigned width, int x) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetCode6C)(p);
    const unsigned char c0 = text[0];
    const auto glyph = static_cast<std::uint16_t>((c0 & 0x80) != 0 ? ((c0 & 0x7Fu) << 8) + text[1] : c0 - 0x26u);
    SetWord(p + 0x16, glyph);
    const auto wb = static_cast<unsigned char>(width);
    p[0xC] = 0;
    p[0xD] = 0;
    if ((wb & 0x80) != 0) p[0xC] = static_cast<unsigned char>(0xC - (wb & 0x7F));
    const unsigned char u0 = p[0xC];
    const auto w7 = static_cast<unsigned char>(wb & 0x7F);
    p[0x1C] = u0;
    p[0x14] = p[0x24] = static_cast<unsigned char>(u0 + w7);
    SetWord(p + 8, static_cast<unsigned>(x));
    SetWord(p + 0x18, static_cast<unsigned>(x));
    SetWord(p + 0x10, static_cast<unsigned>(x) + w7);
    SetWord(p + 0x20, static_cast<unsigned>(x) + w7);
    p[0x15] = 0;
    p[0x1D] = 0xC;
    p[0x25] = 0xC;
    const U top = static_cast<U>(Long(At(at::kRecord3Y))) + 3u;
    SetWord(p + 0xA, top);
    SetWord(p + 0x12, top);
    SetWord(p + 0x1A, top + 0xC);
    SetWord(p + 0x22, top + 0xC);
    p[4] = p[5] = p[6] = 0x80;
    SetWord(p + 0xE, 0x7800u | (clut & 0xF));
    SH_CALL(Gfx_CommitPrim)(1, 0x28);
}

// --- kinds 0x92, 0x11, 0x12, 0x14 ---------------------------------------------------

// original 0x46A3E0 (Effect_KindHandlers[0x92]): unless +0 has bit 6, the
// sprite object +0xB (of Sprite_Objects' 30) moves by the record's words +0x2E
// / +0x30 and takes its +0x29; once that object's +0 bit 0 is clear,
// Effect_Release.
extern "C" void __cdecl EffectKind92_Follow() {
    unsigned char* const s = Sc();
    const unsigned n = s[0xB];
    if (n >= at::kSpriteCount)
        bof3::Fatal("EffectKind92_Follow: sprite object %u past the %u (the original writes past Sprite_Objects)", n,
                    at::kSpriteCount);
    unsigned char* const o = Sprite_Objects + at::kSpriteStride * n;
    if ((s[0] & 0x40) == 0) {
        SetWord(o + 0x2E, Word(o + 0x2E) + Word(s + 0x2E));
        SetWord(o + 0x30, Word(o + 0x30) + Word(s + 0x30));
        o[0x29] = s[0x29];
    }
    if ((o[0] & 1) == 0) SH_CALL(Effect_Release)();
}

// original 0x46A450 (Effect_KindHandlers[0x11]): the draw mode (0, 1, tpage
// Gpu_GetTPage(2, 3, 0x140, 0x140), 0) committed (6, 0xC); a POLY_G4 of the
// screen (0, 0)..(320, 320) as floats, every colour 0, semi-transparent (1),
// committed (2, 0x44).
extern "C" void __cdecl EffectKind11_DrawShade() {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(2, 3, 0x140, 0x140) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    const int left = -static_cast<int>(Widescreen_Fill()), right = 320 + static_cast<int>(Widescreen_Fill());
    F(p + 8, left);
    F(p + 0xC, 0);
    F(p + 0x18, right);
    F(p + 0x1C, 0);
    F(p + 0x28, left);
    F(p + 0x2C, 320);
    F(p + 0x38, right);
    F(p + 0x3C, 320);
    p[4] = p[5] = p[6] = 0;
    p[0x14] = p[0x15] = p[0x16] = 0;
    p[0x24] = p[0x25] = p[0x26] = 0;
    p[0x34] = p[0x35] = p[0x36] = 0;
    SH_CALL(Gpu_SetSemiTrans)(Gfx_PacketNext, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
}

// original 0x46A500 (Effect_KindHandlers[0x12]): the draw mode (0, 1, tpage
// Gpu_GetTPage(1, 2, 0x140, 0), 0) committed (2, 0xC); a POLY_G4 (0, 0)..(320,
// 100), 0xC8 grey above and black below, semi-transparent (set before its
// corners), committed (2, 0x44); the draw mode (0, 0, Gpu_GetTPage(2, 3, 0x140,
// 0x140), 0) committed (2, 0xC).
extern "C" void __cdecl EffectKind12_DrawGradient() {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(1, 2, 0x140, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    const int left = -static_cast<int>(Widescreen_Fill()), right = 320 + static_cast<int>(Widescreen_Fill());
    F(p + 0x2C, 100);
    F(p + 0x3C, 100);
    F(p + 8, left);
    F(p + 0xC, 0);
    F(p + 0x18, right);
    F(p + 0x1C, 0);
    F(p + 0x28, left);
    F(p + 0x38, right);
    p[4] = p[5] = p[6] = 0xC8;
    p[0x14] = p[0x15] = p[0x16] = 0xC8;
    p[0x24] = p[0x25] = p[0x26] = 0;
    p[0x34] = p[0x35] = p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    const unsigned tpage2 = SH_CALL(Gpu_GetTPage)(2, 3, 0x140, 0x140) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage2, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x46A5E0 (Effect_KindHandlers[0x14]): EffectKind14_States[+1], a tail jump.
extern "C" void __cdecl EffectKind14_Run() {
    StepAt(at::kKind14States, Sc()[1], at::kKind14Count, "EffectKind14_Run")();
}

void Effect1B_Inject() {
    if (bof3::WantsShadow("effect_1b")) effect_1b::SelfTest();
    BOF3_INJECT(EffectKind0F_ListOpen);
    BOF3_INJECT(EffectKind0F_ListShow);
    BOF3_INJECT(EffectKind0F_ListClose);
    BOF3_INJECT(EffectKind0F_Child);
    BOF3_INJECT(EffectKind0F_Child0);
    BOF3_INJECT(EffectKind0F_Child0Start);
    BOF3_INJECT(EffectKind0F_Child0Animate);
    BOF3_INJECT(EffectKind0F_Child0Second);
    BOF3_INJECT(EffectKind0F_Child0Tick);
    BOF3_INJECT(EffectKind0F_Child0Back);
    BOF3_INJECT(EffectKind0F_Child1);
    BOF3_INJECT(EffectKind0F_Child1Start);
    BOF3_INJECT(EffectKind0F_Child1Meter);
    BOF3_INJECT(EffectKind0F_Child2);
    BOF3_INJECT(EffectKind0F_Child2Start);
    BOF3_INJECT(EffectKind0F_Child2Gauge);
    BOF3_INJECT(EffectKind0F_Child2Fade);
    BOF3_INJECT(EffectKind0F_Child2Blink);
    BOF3_INJECT(EffectKind0F_Child3);
    BOF3_INJECT(EffectKind0F_Child3Start);
    BOF3_INJECT(EffectKind0F_Child3Grid);
    BOF3_INJECT(EffectKind0F_Child4);
    BOF3_INJECT(EffectKind0F_Child4Start);
    BOF3_INJECT(EffectKind0F_Child4Move);
    BOF3_INJECT(EffectKind0F_DrawCursorPanel);
    BOF3_INJECT(EffectKind0F_Child5);
    BOF3_INJECT(EffectKind0F_Child5Start);
    BOF3_INJECT(EffectKind0F_Child5Move);
    BOF3_INJECT(EffectKind0F_DrawMessageList);
    BOF3_INJECT(EffectKind0F_DrawListFrame);
    BOF3_INJECT(Panel_DrawEdgeQuad);
    BOF3_INJECT(EffectKind0F_DrawCountHeader);
    BOF3_INJECT(EffectKind0F_DrawToggles);
    BOF3_INJECT(EffectKind0F_DrawToggle);
    BOF3_INJECT(EffectKind0F_DrawItemsB);
    BOF3_INJECT(EffectKind0F_DrawScrollMark);
    BOF3_INJECT(EffectKind0F_DrawItemsA);
    BOF3_INJECT(EffectKind0F_DrawEquipped);
    BOF3_INJECT(EffectKind0F_DrawTwinFrame);
    BOF3_INJECT(EffectKind0F_DrawItemFrame);
    BOF3_INJECT(Panel_DrawWindow);
    BOF3_INJECT(Panel_DrawWindowBevel);
    BOF3_INJECT(Panel_DrawWindowEdges);
    BOF3_INJECT(EffectKind0F_DrawGlyph);
    BOF3_INJECT(EffectKind92_Follow);
    BOF3_INJECT(EffectKind11_DrawShade);
    BOF3_INJECT(EffectKind12_DrawGradient);
    BOF3_INJECT(EffectKind14_Run);
}

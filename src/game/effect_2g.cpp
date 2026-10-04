// Round thirteen group E2G (docs/effect_2g.md): the 56 functions of
// analysis/round13_cut.tsv's group E2G, 0x47DBE0..0x47FD92, and two starts the
// cut does not list (the sub-kind's shared draw 0x47E120, kind 0x5E's
// dispatcher 0x47F5D0), each read with capstone to its last instruction.
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and
// the states it names. What each kind is, as far as the code says:
//
//   kind 0x66   a window (area 135's choice spawns it, +6 the answer): it grows
//               over eight frames, stands with the message MsgBox_FrameTask
//               types and two or three textured pieces, and shrinks away
//   kind 0x18, sub-kind 0x20   a pair of textured strips that lower and rise
//               with story flag 0x2A (Cond_ByteFE the step), MoveCmd_TestFB
//               at each end
//   kind 0x15   the second party member's side of a timed exchange: it waits
//               on the cue byte 0x90384A, animates ObjTrio record 1, and raises
//               the cue after random waits (area 145, chapter 3)
//   kind 0x54   a field object's side of the same exchange (area 8, area 135):
//               the object +0xB names answers the cue, the count 0x90384B up on
//               a hit
//   kind 0x55   a thirty-second clock and the count, then the end (bit 7 of
//               the count; bit 7 of the cue too at a count of 16 or more)
//   kind 0x57   a ring of sixteen shaded quads round the screen's centre (in
//               area 2 it stays)
//   kind 0x5A   a glowing cylinder placed on the ground at (10, 0x63)
//   kind 0x5B   area 75's two beams: a textured quad from a point to the ground
//               beside it, brightening and fading
//   kinds 0x5D, 0x5E, 0x5F   dispatchers only (their states are catalog part 6
//               rows, no group's this round)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes a table or the object pool past its records,
// ours aborts with a message (docs/effect_2g.md section 6).
#include "game/effect_2g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2g_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void StoreFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild dword; fstp dword
    std::memcpy(p, &f, sizeof f);
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2g.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// Sprite_Objects record `index` (the effect record's +0xB), as the originals
// compute it: unchecked where only its address is stored, checked by Object()
// where it is read, written or animated.
unsigned char* ObjectAddress(unsigned index) { return Sprite_Objects + index * at::kObjectStride; }
unsigned char* Object(const char* who, unsigned index) {
    if (index >= at::kObjectCount)
        bof3::Fatal("%s: +0xB is %u, past the thirty Sprite_Objects records - the original reads and writes what "
                    "follows (docs/effect_2g.md section 6)",
                    who, index);
    return ObjectAddress(index);
}

bool Over() { return (B(at::kScore) & 0x80) == 0x80; }

// ===========================================================================
// Kind 0x66: Effect_KindHandlers[0x66] (0x6554E8), EffectKind66_States (six)
// ===========================================================================

// The growing / shrinking outline of the window at frame count b (+9):
// Window_DrawOutline(x, y, w, h), w = 25 b, h = 6 b, centred on the window's
// rectangle - x = (0xC8 - w) / 2 + the dword 0x654750 (both its words), y =
// (0x30 - h) / 2 with the word 0x654752 added to its low half. The original
// pushes w and h as the whole registers it multiplied, whose upper halves hold
// a caller's leftover; Window_DrawOutline reads their low bytes (the fuzz
// masks them so).
void Outline(unsigned b) {
    const U w = 25u * b, h = 6u * b;
    const U qy = static_cast<U>((0x30 - S16(h)) / 2);
    const U y = (qy & 0xFFFF0000u) | ((qy + W(at::kKind66Rect + 2)) & 0xFFFFu);
    const U x = static_cast<U>((0xC8 - S16(w)) / 2) + UL(at::kKind66Rect);
    SH_CALL(Window_DrawOutline)(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h));
}

// The window at its rectangle and the message's step, when +6 is set.
void Window() {
    if (S()[6] == 0) return;
    SH_CALL(Window_DrawFrame)(static_cast<int>(W(at::kKind66Rect)), static_cast<int>(W(at::kKind66Rect + 2)),
                              B(at::kKind66Rect + 4), B(at::kKind66Rect + 6));
    SH_CALL(EffectKind66_Message)();
}

}  // namespace

// original 0x47DBE0 (Effect_KindHandlers[0x66], hidden in E2F's 0x47DAC0):
// jmp [EffectKind66_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind66_Run(void) {
    Dispatch("EffectKind66_Run", AddressOf(EffectKind66_States), EffectKind66_States_count, 1);
}

// original 0x47DC00 (state 0): no message (0x7DEE48 = 0xFFFF), +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind66_Start(void) {
    SetWord(At(at::kMessageWord), 0xFFFF);
    S()[9] = 0;
    S()[1] = 1;
}

// original 0x47DC20 (state 1): +9 up; with +6 set the outline at that count
// (Sprite_Current read again after it); at a count of 8, +1 = 2.
extern "C" void __cdecl EffectKind66_Open(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[6] != 0) {
        Outline(s[9]);
        s = S();
    }
    if (s[9] == 8) s[1] = 2;
}

// original 0x47DCA0 (state 2): the window and the message (with +6), then
// piece 0 at place 0 and piece 2 at place 1.
extern "C" void __cdecl EffectKind66_Show2(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(0, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 1);
}

// original 0x47DCF0 (state 3): as state 2, piece 1 at place 0 and piece 2 at
// place 2.
extern "C" void __cdecl EffectKind66_Show3(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(1, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 2);
}

// original 0x47DD40 (state 4): as state 2, and piece 3 at place 1.
extern "C" void __cdecl EffectKind66_Show4(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(0, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 1);
    SH_CALL(EffectKind66_DrawPiece)(3, 1);
}

// original 0x47DDA0 (state 5): with +6 the outline at +9 (Sprite_Current read
// again after it); +9 at 0: a tail jump to Effect_Release; else +9 down.
extern "C" void __cdecl EffectKind66_Close(void) {
    unsigned char* s = S();
    if (s[6] != 0) {
        Outline(s[9]);
        s = S();
    }
    if (s[9] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x47DE10 (called by states 2..4): a message set (0x7DEE48 not
// 0xFFFF): MsgBox_FrameTask (its answer unread); then MsgBoxState's first byte
// 2 -> 7 and its +6 = 0xA; 7 -> +6 = 0xA.
extern "C" void __cdecl EffectKind66_Message(void) {
    if (W(at::kMessageWord) == 0xFFFF) return;
    SH_CALL(MsgBox_FrameTask)();
    const unsigned char state = B(bof3::addr::MsgBoxState);
    if (state == 2) {
        B(bof3::addr::MsgBoxState) = 7;
        B(at::kMsgBoxStep) = 0xA;
        return;
    }
    if (state == 7) B(at::kMsgBoxStep) = 0xA;
}

// original 0x47DE50 (called by states 2..4; cdecl): a POLY_FT4 at
// Gfx_PacketNext - the corners the place record (0x654770 + 8 * place) gives
// as floats (x, y, x + w, y + h), the texture corners the piece record
// (0x654788 + 4 * piece) gives, shade 0x80, CLUT Gpu_GetClut(0, 0x1E8), page
// Gpu_GetTPage(1, 0, 0x240, 0x100) - committed to slot 1 (0x48). The low byte
// of each argument is read; past the three places or the four pieces the
// original reads the .data after them: ours aborts.
extern "C" void __cdecl EffectKind66_DrawPiece(unsigned piece, unsigned place) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const unsigned r = place & 0xFF, t = piece & 0xFF;
    if (r >= at::kKind66PlaceCount || t >= at::kKind66PieceCount)
        bof3::Fatal("EffectKind66_DrawPiece: piece %u, place %u - past the four pieces of 0x654788 or the three places of "
                    "0x654770 (docs/effect_2g.md section 6)",
                    t, r);
    const U place_at = at::kKind66Places + 8 * r;
    const std::int32_t x = S16(W(place_at)), y = S16(W(place_at + 2));
    const std::int32_t w = S16(W(place_at + 4)), h = S16(W(place_at + 6));
    StoreFloat(p + 8, x);
    StoreFloat(p + 0xC, y);
    StoreFloat(p + 0x18, x + w);
    StoreFloat(p + 0x1C, y);
    StoreFloat(p + 0x28, x);
    StoreFloat(p + 0x2C, y + h);
    StoreFloat(p + 0x38, x + w);
    StoreFloat(p + 0x3C, y + h);
    const U uv = at::kKind66Pieces + 4 * t;
    const unsigned char u = B(uv), v = B(uv + 1), du = B(uv + 2), dv = B(uv + 3);
    p[0x14] = u;
    p[0x15] = v;
    p[0x24] = static_cast<unsigned char>(u + du);
    p[0x25] = v;
    p[0x34] = u;
    p[0x35] = static_cast<unsigned char>(v + dv);
    p[0x44] = static_cast<unsigned char>(u + du);
    p[4] = 0x80;
    p[0x45] = static_cast<unsigned char>(v + dv);
    p[5] = 0x80;
    p[6] = 0x80;
    SetWord(p + 0x16, SH_CALL(Gpu_GetClut)(0, 0x1E8));
    SetWord(p + 0x26, SH_CALL(Gpu_GetTPage)(1, 0, 0x240, 0x100));
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ===========================================================================
// Kind 0x18's sub-kind 0x20: EffectKind18_States[0x20] (0x6540EC),
// EffectKind18Sub20_States (five) by +2
// ===========================================================================

// original 0x47DFD0 (EffectKind18_States[0x20], hidden in 0x47DE50):
// jmp [EffectKind18Sub20_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub20_Run(void) {
    Dispatch("EffectKind18Sub20_Run", AddressOf(EffectKind18Sub20_States), EffectKind18Sub20_States_count, 2);
}

// original 0x47DFF0 (sub-state 0): story flag 0x2A set: the height +0x3C =
// -0x180 and +2 = 3; clear: +0x3C = -0x90 and +2 up. Then the draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub20_Start(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub20Flag)) {
        SetUL(S() + 0x3C, 0xFFFFFE80u);
        S()[2] = 3;
    } else {
        SetUL(S() + 0x3C, 0xFFFFFF70u);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E040 (sub-state 1): Cond_ByteFE = 1; once story flag 0x2A is
// set, sound 0x202 and +2 up (read after the sound). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub20_WaitSet(void) {
    Cond_ByteFE = 1;
    if (!SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub20Flag)) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x47E070 (sub-state 2): Cond_ByteFE = 0; +0x3C down 8; at -0x180 or
// below, MoveCmd_TestFB(0x27, 0x2F) (its answer unread) and +2 up. Then the draw.
extern "C" void __cdecl EffectKind18Sub20_Lower(void) {
    unsigned char* s = S();
    Cond_ByteFE = 0;
    SetUL(s + 0x3C, UL(s + 0x3C) - 8u);
    s = S();
    if (static_cast<std::int32_t>(UL(s + 0x3C)) <= -0x180) {
        SH_CALL(MoveCmd_TestFB)(0x27, 0x2F);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E0B0 (sub-state 3): Cond_ByteFE = 2; once story flag 0x2A is
// clear, MoveCmd_TestFB(0x27, 0x2F), sound 0x202 and +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub20_WaitClear(void) {
    Cond_ByteFE = 2;
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub20Flag)) return;
    SH_CALL(MoveCmd_TestFB)(0x27, 0x2F);
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x47E0F0 (sub-state 4): Cond_ByteFE = 0; +0x3C up 8; at -0x90 or
// above, +2 = 1. Then the draw (the jmp to 0x47E120, the code after it).
extern "C" void __cdecl EffectKind18Sub20_Raise(void) {
    unsigned char* s = S();
    Cond_ByteFE = 0;
    SetUL(s + 0x3C, UL(s + 0x3C) + 8u);
    s = S();
    if (static_cast<std::int32_t>(UL(s + 0x3C)) >= -0x90) s[2] = 1;
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E120 (the tail sub-states 0, 2 and 4 jump to; inside 0x47E0F0's
// extent): four rows (the z 0x300000 + 0x20000 i, i = 0..3) of two POLY_FT4s,
// shaded, their vertices built in Prim_VertexScratch - x -0x2940 (then -0x2A40
// for the second's far pair), y 0x100 i - 0x2740 / - 0x2840, z the height's
// low word +0x3C and 0x100 above it (Sprite_Current read after the primitive's
// set-up, each quad) - projected by Gte_RotTransPers4, depths by
// Gte_PrimDepths4_10, textures 0x23500125 and 0x23800126, each linked at
// (0x2C0000, z) with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub20_Draw(void) {
    constexpr U kNear = 0xFFFFD6C0u, kFar = 0xFFFFD5C0u;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    auto vertex = [v](unsigned i) { return reinterpret_cast<const short*>(v + 8 * i); };
    U row = 0;
    for (U z = 0x300000; z < 0x380000; z += 0x20000, ++row) {
        unsigned char* p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const U y0 = (row << 8) - 0x2740u, y1 = (row << 8) + 0xFFFFD7C0u;
        SetWord(v + 0x18, kNear);
        SetWord(v + 0x10, kNear);
        SetWord(v + 8, kNear);
        SetWord(v + 0x1A, y1);
        SetWord(v + 0xA, y1);
        unsigned char* s = S();
        SetWord(v, kNear);
        SetWord(v + 0x12, y0);
        SetWord(v + 2, y0);
        SetWord(v + 0xC, Word(s + 0x3C));
        SetWord(v + 4, Word(s + 0x3C));
        SetWord(v + 0x1C, Word(s + 0x3C) + 0x100u);
        SetWord(v + 0x14, Word(s + 0x3C) + 0x100u);
        long depth;
        SH_CALL(Gte_RotTransPers4)(vertex(0), vertex(1), vertex(2), vertex(3), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(0x23500125, p, 1);
        SH_CALL(MapView_LinkPrimAt)(0x2C0000, z, 0, 0x48);
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetWord(v + 0x18, kFar);
        SetWord(v + 0x10, kFar);
        s = S();
        SetWord(v + 0x1C, Word(s + 0x3C));
        SetWord(v + 0x14, Word(s + 0x3C));
        SH_CALL(Gte_RotTransPers4)(vertex(3), vertex(1), vertex(2), vertex(0), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(0x23800126, p, 1);
        SH_CALL(MapView_LinkPrimAt)(0x2C0000, z, 0, 0x48);
    }
}

// ===========================================================================
// Kind 0x15: Effect_KindHandlers[0x15] (0x6553A4), EffectKind15_States (ten)
// ===========================================================================

namespace {

unsigned char* Member1() { return At(at::kMember1); }

// Field_State and Sprite_Current to ObjTrio record 1 (the second member).
void OnMember1() {
    Field_State = Member1();
    Sprite_Current = Member1();
}

// The end (bit 7 of the count 0x90384B): +1 = 9.
bool EndedTo(unsigned char state) {
    if (!Over()) return false;
    S()[1] = state;
    return true;
}

// A hit to take (0x903849 = 1): the cue 5, the hit 2, +1 = 6.
bool HitTaken() {
    if (B(at::kHit) != 1) return false;
    unsigned char* const s = S();
    B(at::kCue) = 5;
    B(at::kHit) = 2;
    s[1] = 6;
    return true;
}

}  // namespace

// original 0x47E2D0 (Effect_KindHandlers[0x15], hidden in 0x47DE50):
// jmp [EffectKind15_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind15_Run(void) {
    Dispatch("EffectKind15_Run", AddressOf(EffectKind15_States), EffectKind15_States_count, 1);
}

// original 0x47E2F0 (state 0): +9 = 0, the wait 0x67625C = 0, +1 = 1.
extern "C" void __cdecl EffectKind15_Start(void) {
    S()[9] = 0;
    B(at::kKind15Wait) = 0;
    S()[1] = 1;
}

// original 0x47E310 (state 1): the end: +1 = 9. Else member 1's +0x124 bit 6
// set, animation 0x10 on it (Field_State and Sprite_Current its record), the
// cue 1, Sprite_Current back, +1 = 2.
extern "C" void __cdecl EffectKind15_Begin(void) {
    if (EndedTo(9)) return;
    const auto flags = static_cast<unsigned char>(B(at::kMember1Flags) | 0x40);
    unsigned char* const self = S();
    OnMember1();
    B(at::kMember1Flags) = flags;
    SH_CALL(Sprite_SetAnimation)(0x10);
    B(at::kCue) = 1;
    Sprite_Current = self;
    self[1] = 2;
}

// original 0x47E370 (state 2): the end: +1 = 9; a hit: +1 = 6. Else Field_State
// and Sprite_Current to member 1; its animation 4 run through (+0x58 4, +0x4A
// 1): the wait from 0x6547AC by Rand() & 7, Sprite_Current back, +1 = 3.
// Otherwise the original returns with Sprite_Current left on member 1 (section 7).
extern "C" void __cdecl EffectKind15_WaitPose4(void) {
    if (EndedTo(9) || HitTaken()) return;
    unsigned char* const self = S();
    const bool pose = W(at::kMember1Pose) == 4;
    OnMember1();
    if (!pose || B(at::kMember1PoseDone) != 1) return;
    const U r = static_cast<U>(SH_CALL(Rand)()) & 7u;
    Sprite_Current = self;
    B(at::kKind15Wait) = B(at::kKind15Delays8 + r);
    self[1] = 3;
}

// original 0x47E410 (state 3): the end: +1 = 9; a hit: +1 = 6. Else the wait
// down; at 1 or 0: the cue 2, animation 1 on member 1, the wait from 0x6547B8
// by Rand() & 3, +1 = 4.
extern "C" void __cdecl EffectKind15_Countdown(void) {
    if (EndedTo(9) || HitTaken()) return;
    const auto wait = static_cast<unsigned char>(B(at::kKind15Wait) - 1);
    B(at::kKind15Wait) = wait;
    if (wait > 1) return;
    unsigned char* const self = S();
    B(at::kCue) = 2;
    OnMember1();
    SH_CALL(Sprite_SetAnimation)(1);
    const U r = static_cast<U>(SH_CALL(Rand)()) & 3u;
    Sprite_Current = self;
    B(at::kKind15Wait) = B(at::kKind15Delays4 + r);
    self[1] = 4;
}

// original 0x47E4B0 (states 4 and 8): the end: +1 = 9; the cue at 3: +1 = 5.
extern "C" void __cdecl EffectKind15_WaitCue3(void) {
    if (EndedTo(9)) return;
    if (B(at::kCue) == 3) S()[1] = 5;
}

// original 0x47E4E0 (state 5): the end: +1 = 9. Else the cue 0; the timer word
// 0x8034E6 at 0xFF: it is cleared and the wait taken as 2. The wait down; at 1
// or 0 the hit cleared and +1 = 1.
extern "C" void __cdecl EffectKind15_Cooldown(void) {
    if (EndedTo(9)) return;
    const bool mark = W(at::kTimerWord) == 0xFF;
    B(at::kCue) = 0;
    unsigned char wait;
    if (mark) {
        SetWord(At(at::kTimerWord), 0);
        wait = 2;
    } else {
        wait = B(at::kKind15Wait);
    }
    wait = static_cast<unsigned char>(wait - 1);
    B(at::kKind15Wait) = wait;
    if (wait > 1) return;
    unsigned char* const s = S();
    B(at::kHit) = 0;
    s[1] = 1;
}

// original 0x47E540 (state 6): +9 = 0; the end: +1 = 9. Else member 1's bit 6
// of +0x124 set, sound 0x20E and animation 0x57 on it, +1 = 7.
extern "C" void __cdecl EffectKind15_Hit(void) {
    S()[9] = 0;
    if (EndedTo(9)) return;
    const auto flags = static_cast<unsigned char>(B(at::kMember1Flags) | 0x40);
    unsigned char* const self = S();
    OnMember1();
    B(at::kMember1Flags) = flags;
    SH_CALL(Sound_PlayEffect)(0x20E);
    SH_CALL(Sprite_SetAnimation)(0x57);
    Sprite_Current = self;
    self[1] = 7;
}

// original 0x47E5B0 (state 7): the end: +1 = 9. Else Field_State and
// Sprite_Current to member 1; its animation 8 run through: animation 1, the
// wait from 0x6547B8 by Rand() & 3, Sprite_Current back, +1 = 8. Otherwise, as
// state 2, Sprite_Current left on member 1.
extern "C" void __cdecl EffectKind15_WaitPose8(void) {
    if (EndedTo(9)) return;
    unsigned char* const self = S();
    const bool pose = W(at::kMember1Pose) == 8;
    OnMember1();
    if (!pose || B(at::kMember1PoseDone) != 1) return;
    SH_CALL(Sprite_SetAnimation)(1);
    const U r = static_cast<U>(SH_CALL(Rand)()) & 3u;
    Sprite_Current = self;
    B(at::kKind15Wait) = B(at::kKind15Delays4 + r);
    self[1] = 8;
}

// original 0x47E630 (state 9): animation 1 on member 1; bit 6 of +0x124 of the
// record Field_State names after the call cleared; Sprite_Current back and
// Effect_Release.
extern "C" void __cdecl EffectKind15_End(void) {
    unsigned char* const self = S();
    OnMember1();
    SH_CALL(Sprite_SetAnimation)(1);
    unsigned char* const member = Field_State;
    member[0x124] = static_cast<unsigned char>(member[0x124] & 0xBF);
    Sprite_Current = self;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x54: Effect_KindHandlers[0x54] (0x6554A0), EffectKind54_States (15)
// ===========================================================================

// original 0x47E680 (Effect_KindHandlers[0x54], hidden in 0x47DE50):
// jmp [EffectKind54_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind54_Run(void) {
    Dispatch("EffectKind54_Run", AddressOf(EffectKind54_States), EffectKind54_States_count, 1);
}

// original 0x47E6A0 (state 0; also kind 0x67's state 0, 0x6549EC): +9 = 0,
// +1 = 1.
extern "C" void __cdecl EffectKind54_Start(void) {
    S()[9] = 0;
    S()[1] = 1;
}

// original 0x47E6C0 (state 1): the end: +1 = 0xE. The cue at 1: the object
// +0xB names loses bit 6 of its +0, +1 = 2.
extern "C" void __cdecl EffectKind54_Arm(void) {
    if (EndedTo(0xE)) return;
    if (B(at::kCue) != 1) return;
    unsigned char* const o = Object("EffectKind54_Arm", S()[0xB]);
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    S()[1] = 2;
}

// original 0x47E710 (state 2): Sprite_Current to the object; the end: +1 =
// 0xE. The cue at 2: animation 3, +1 = 3; at 5: animation 3, +1 = 0xA.
// Sprite_Current back.
extern "C" void __cdecl EffectKind54_Cue(void) {
    unsigned char* const self = S();
    const unsigned index = self[0xB];
    const bool over = Over();
    Sprite_Current = ObjectAddress(index);
    if (over) {
        self[1] = 0xE;
        Sprite_Current = self;
        return;
    }
    const unsigned char cue = B(at::kCue);
    if (cue == 2 || cue == 5) {
        Object("EffectKind54_Cue", index);
        SH_CALL(Sprite_SetAnimation)(3);
        self[1] = cue == 2 ? 3 : 0xA;
    }
    Sprite_Current = self;
}

// original 0x47E790 (state 3): Sprite_Current to the object; the end: +1 =
// 0xE. A hit to take: the count up, the hit 2, sound 0x20D; the record
// Sprite_Current names after it at animation 4 or below (+0x58): the timer word
// 0x8034E6 = 0xFF; animation 2, +1 = 7. Else the object's animation 8 run
// through: animation 4, +1 = 4. Sprite_Current back.
extern "C" void __cdecl EffectKind54_Strike(void) {
    unsigned char* const self = S();
    const unsigned index = self[0xB];
    const bool over = Over();
    Sprite_Current = ObjectAddress(index);
    if (over) {
        self[1] = 0xE;
        Sprite_Current = self;
        return;
    }
    if (B(at::kHit) == 1) {
        Object("EffectKind54_Strike", index);
        const auto score = static_cast<unsigned char>(B(at::kScore) + 1);
        B(at::kHit) = 2;
        B(at::kScore) = score;
        SH_CALL(Sound_PlayEffect)(0x20D);
        if (Word(S() + 0x58) <= 4) SetWord(At(at::kTimerWord), 0xFF);
        SH_CALL(Sprite_SetAnimation)(2);
        self[1] = 7;
        Sprite_Current = self;
        return;
    }
    unsigned char* const o = Object("EffectKind54_Strike", index);
    if (Word(o + 0x58) == 8 && o[0x4A] == 1) {
        SH_CALL(Sprite_SetAnimation)(4);
        self[1] = 4;
    }
    Sprite_Current = self;
}

namespace {

// States 4, 7 and 0xB: Sprite_Current to the object; the end: +1 = 0xE; the
// object's animation `pose` run through (+0x58, +0x4A 1): +1 = next.
// Sprite_Current back.
void WaitPose(const char* who, unsigned pose, unsigned char next) {
    unsigned char* const self = S();
    const unsigned index = self[0xB];
    const bool over = Over();
    Sprite_Current = ObjectAddress(index);
    if (over) {
        self[1] = 0xE;
        Sprite_Current = self;
        return;
    }
    unsigned char* const o = Object(who, index);
    if (Word(o + 0x58) == pose && o[0x4A] == 1) self[1] = next;
    Sprite_Current = self;
}

}  // namespace

// original 0x47E850 (state 4): animation 2 run through: +1 = 5.
extern "C" void __cdecl EffectKind54_WaitPose2(void) { WaitPose("EffectKind54_WaitPose2", 2, 5); }

// original 0x47E8A0 (state 7): animation 8 run through: +1 = 8.
extern "C" void __cdecl EffectKind54_WaitPose8(void) { WaitPose("EffectKind54_WaitPose8", 8, 8); }

// original 0x47E900 (states 5, 8 and 0xC): the end: +1 = 0xE. Else the object's
// bit 6 of +0 set, the cue 3, animation 1 on the object, +1 = 1.
extern "C" void __cdecl EffectKind54_Rearm(void) {
    if (EndedTo(0xE)) return;
    unsigned char* o = Object("EffectKind54_Rearm", S()[0xB]);
    o[0] = static_cast<unsigned char>(o[0] | 0x40);
    unsigned char* const self = S();
    B(at::kCue) = 3;
    Sprite_Current = Object("EffectKind54_Rearm", self[0xB]);
    SH_CALL(Sprite_SetAnimation)(1);
    self[1] = 1;
    Sprite_Current = self;
}

// original 0x47E980 (state 0xA): Sprite_Current to the object; the end: +1 =
// 0xE; its animation 8 run through: animation 4, +1 = 0xB. Sprite_Current back.
extern "C" void __cdecl EffectKind54_Recoil(void) {
    unsigned char* const self = S();
    const unsigned index = self[0xB];
    const bool over = Over();
    Sprite_Current = ObjectAddress(index);
    if (over) {
        self[1] = 0xE;
        Sprite_Current = self;
        return;
    }
    unsigned char* const o = Object("EffectKind54_Recoil", index);
    if (Word(o + 0x58) == 8 && o[0x4A] == 1) {
        SH_CALL(Sprite_SetAnimation)(4);
        self[1] = 0xB;
    }
    Sprite_Current = self;
}

// original 0x47E9E0 (state 0xB): animation 2 run through: +1 = 0xC.
extern "C" void __cdecl EffectKind54_WaitPose2B(void) { WaitPose("EffectKind54_WaitPose2B", 2, 0xC); }

// original 0x47EA30 (state 0xE): the object's bit 6 of +0 set, animation 1 on
// it, Sprite_Current back and Effect_Release.
extern "C" void __cdecl EffectKind54_End(void) {
    unsigned char* o = Object("EffectKind54_End", S()[0xB]);
    o[0] = static_cast<unsigned char>(o[0] | 0x40);
    unsigned char* const self = S();
    Sprite_Current = Object("EffectKind54_End", self[0xB]);
    SH_CALL(Sprite_SetAnimation)(1);
    Sprite_Current = self;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x55: Effect_KindHandlers[0x55] (0x6554A4), EffectKind55_States (three)
// ===========================================================================

// original 0x47EA90 (Effect_KindHandlers[0x55], hidden in 0x47DE50):
// jmp [EffectKind55_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind55_Run(void) {
    Dispatch("EffectKind55_Run", AddressOf(EffectKind55_States), EffectKind55_States_count, 1);
}

// original 0x47EAB0 (state 0): the frames +0x5D = 0, the seconds +0x5E = 30,
// +1 up.
extern "C" void __cdecl EffectKind55_Start(void) {
    S()[0x5D] = 0;
    S()[0x5E] = 0x1E;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47EAD0 (state 1): the clock and the count drawn; the frames down,
// below 0 (s8) back to 0x1D and the seconds down; the seconds below 0: both 0
// and +1 up.
extern "C" void __cdecl EffectKind55_Tick(void) {
    SH_CALL(EffectKind55_DrawTime)();
    SH_CALL(EffectKind55_DrawCount)();
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] - 1);
    if (S8(s[0x5D]) < 0) {
        s[0x5D] = 0x1D;
        s[0x5E] = static_cast<unsigned char>(s[0x5E] - 1);
    }
    if (S8(s[0x5E]) < 0) {
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x47EB30 (state 2): the count at 16 or more: bit 7 set on the count
// and on the cue; else on the count only. Music_FadeOutStop(0xA), a tail jump
// to Effect_Release.
extern "C" void __cdecl EffectKind55_Finish(void) {
    if (B(at::kScore) >= 0x10) {
        const auto score = static_cast<unsigned char>(B(at::kScore) | 0x80);
        const auto cue = static_cast<unsigned char>(B(at::kCue) | 0x80);
        B(at::kScore) = score;
        B(at::kCue) = cue;
    } else {
        B(at::kScore) = static_cast<unsigned char>(B(at::kScore) | 0x80);
    }
    SH_CALL(Music_FadeOutStop)(0xA);
    SH_CALL(Effect_Release)();
}

// original 0x47EB90 (called by state 1): a box (0x586160(0x78, 0x38, 0x46, 0x14,
// 1)); the seconds (s8 +0x5E) and the hundredths (s8 +0x5D * 100 / 30, toward
// zero) printed by the format 0x654830 into the text scratch, drawn
// (Text_DrawFont12 at (0x7D, 0x3B)); then the scratch made "." and drawn at
// (0x95, 0x39) and (0x95, 0x3D) - a colon.
extern "C" void __cdecl EffectKind55_DrawTime(void) {
    SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBox)(0x78, 0x38, 0x46, 0x14, 1);
    unsigned char* const s = S();
    const int hundredths = S8(s[0x5D]) * 100 / 30;
    const int seconds = S8(s[0x5E]);
    char* const text = reinterpret_cast<char*>(At(at::kText));
    SH_CALL(Crt_sprintf)(text, reinterpret_cast<const char*>(At(at::kKind55TimeFormat)), seconds, hundredths);
    SH_CALL(Text_DrawFont12)(0x7D, 0x3B, 0, reinterpret_cast<const unsigned char*>(text));
    text[0] = '.';
    text[1] = 0;
    SH_CALL(Text_DrawFont12)(0x95, 0x39, 0, reinterpret_cast<const unsigned char*>(text));
    SH_CALL(Text_DrawFont12)(0x95, 0x3D, 0, reinterpret_cast<const unsigned char*>(text));
}

// original 0x47EC30 (called by state 1): a box (0x586160(0xC8, 0xA0, 0x2E,
// 0x14, 1)); the count 0x90384B printed by Boss26Fx_CountFormat and drawn at
// (0xCD, 0xA3); the text 0x66A094 at (0xE5, 0xA4) by Text_DrawAt (colour 0, all
// of it).
extern "C" void __cdecl EffectKind55_DrawCount(void) {
    SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBox)(0xC8, 0xA0, 0x2E, 0x14, 1);
    char* const text = reinterpret_cast<char*>(At(at::kText));
    SH_CALL(Crt_sprintf)(text, Boss26Fx_CountFormat, static_cast<unsigned>(B(at::kScore)));
    SH_CALL(Text_DrawFont12)(0xCD, 0xA3, 0, reinterpret_cast<const unsigned char*>(text));
    SH_CALL(Text_DrawAt)(0xE5, 0xA4, 0, 0xFF, At(at::kKind55Label));
}

// ===========================================================================
// Kind 0x57: Effect_KindHandlers[0x57] (0x6554AC), EffectKind57_States (two)
// ===========================================================================

// original 0x47ECA0 (Effect_KindHandlers[0x57], hidden in 0x47EC30):
// jmp [EffectKind57_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind57_Run(void) {
    Dispatch("EffectKind57_Run", AddressOf(EffectKind57_States), EffectKind57_States_count, 1);
}

// original 0x47ECC0 (state 0): the ring drawn; outside area 2, +1 up (read
// after the draw).
extern "C" void __cdecl EffectKind57_Show(void) {
    SH_CALL(EffectKind57_DrawRing)();
    if (Game_AreaNumber != 2) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47ECE0 (state 1): a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind57_Release(void) { SH_CALL(Effect_Release)(); }

// original 0x47ECF0 (called by state 0): a draw mode (page Gpu_GetTPage(0, 2,
// 0x3C0, 0x100), dtd 1) committed (1, 0xC); then sixteen POLY_G4s,
// semi-transparent, round the centre (0xA0, 0x78) at angles 0x100 k: the inner
// edge (80 cos, 60 sin) >> 12, the outer (240 cos, 180 sin) >> 12, each point's
// words sign-extended from their low 16 bits; each quad the previous angle's
// pair and this one's, black inside and white outside, committed (1, 0x44).
// The first quad's previous pair is (240, 120) - (400, 120).
extern "C" void __cdecl EffectKind57_DrawRing(void) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFF, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    float inner_x = 240.0f, inner_y = 120.0f, outer_x = 400.0f, outer_y = 120.0f;
    U angle = 0;
    for (int n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        angle += 0x100;
        std::memcpy(p + 8, &inner_x, 4);
        std::memcpy(p + 0xC, &inner_y, 4);
        std::memcpy(p + 0x18, &outer_x, 4);
        std::memcpy(p + 0x1C, &outer_y, 4);
        const int a = static_cast<int>(angle & 0xFFFF);
        const U x1 = static_cast<U>(SH_CALL(Math_Cos)(a)) * 80u;
        const U ix = static_cast<U>(static_cast<std::int32_t>(x1) >> 12) + 0xA0u;
        const U y1 = static_cast<U>(SH_CALL(Math_Sin)(a)) * 60u;
        const U iy = static_cast<U>(static_cast<std::int32_t>(y1) >> 12) + 0x78u;
        const U x2 = static_cast<U>(SH_CALL(Math_Cos)(a)) * 240u;
        const U ox = static_cast<U>(static_cast<std::int32_t>(x2) >> 12) + 0xA0u;
        const U y2 = static_cast<U>(SH_CALL(Math_Sin)(a)) * 180u;
        const U oy = static_cast<U>(static_cast<std::int32_t>(y2) >> 12) + 0x78u;
        inner_x = static_cast<float>(S16(ix));
        std::memcpy(p + 0x28, &inner_x, 4);
        inner_y = static_cast<float>(S16(iy));
        std::memcpy(p + 0x2C, &inner_y, 4);
        outer_x = static_cast<float>(S16(ox));
        std::memcpy(p + 0x38, &outer_x, 4);
        p[4] = 0;
        p[5] = 0;
        p[6] = 0;
        outer_y = static_cast<float>(S16(oy));
        std::memcpy(p + 0x3C, &outer_y, 4);
        p[0x14] = 0xFF;
        p[0x15] = 0xFF;
        p[0x16] = 0xFF;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        p[0x34] = 0xFF;
        p[0x35] = 0xFF;
        p[0x36] = 0xFF;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// ===========================================================================
// Kind 0x5A: Effect_KindHandlers[0x5A] (0x6554B8), EffectKind5A_States (two)
// ===========================================================================

// original 0x47EEA0 (Effect_KindHandlers[0x5A], hidden in 0x47ECF0):
// jmp [EffectKind5A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind5A_Run(void) {
    Dispatch("EffectKind5A_Run", AddressOf(EffectKind5A_States), EffectKind5A_States_count, 1);
}

// original 0x47EEC0 (state 0; also kind 0x42's state 0, 0x65449C): the point
// x = 0xA0000, z = 0x630000; the height +0x3C the ground's there
// (AreaMap_Elevation, its low word << 16); +1 up.
extern "C" void __cdecl EffectKind5A_Place(void) {
    SetUL(S() + 0x34, 0xA0000);
    SetUL(S() + 0x38, 0x630000);
    unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38)));
    SetUL(S() + 0x3C, static_cast<U>(S16(static_cast<U>(ground))) << 16);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47EF10 (state 1): Area146_DrawGlowCylinder at the record's point
// (+0x34, +0x38, +0x3C copied to the stack).
extern "C" void __cdecl EffectKind5A_Draw(void) {
    unsigned char* const s = S();
    const long point[3] = {static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38)), static_cast<long>(UL(s + 0x3C))};
    SH_CALL(Area146_DrawGlowCylinder)(point);
}

// ===========================================================================
// Kind 0x5B: Effect_KindHandlers[0x5B] (0x6554BC), EffectKind5B_States (three)
// ===========================================================================

// original 0x47EF40 (Effect_KindHandlers[0x5B], hidden in 0x47ECF0):
// jmp [EffectKind5B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind5B_Run(void) {
    Dispatch("EffectKind5B_Run", AddressOf(EffectKind5B_States), EffectKind5B_States_count, 1);
}

// original 0x47EF60 (state 0): area 75's slot +0xB (0x93C34E + it) = the
// record's index; +0xA = 0x80, the dword +0xC = 0, +6 = 0x1E, +0x5D = 0x80, +1
// up. A slot past the two: ours aborts (the original writes on into area 75's
// cells).
extern "C" void __cdecl EffectKind5B_Start(void) {
    unsigned char* const s = S();
    const unsigned slot = s[0xB];
    if (slot >= at::kArea75EffectSlotCount)
        bof3::Fatal("EffectKind5B_Start: +0xB is %u, past area 75's two effect slots 0x93C34E - the original writes the "
                    "byte after them (docs/effect_2g.md section 6)",
                    slot);
    B(at::kArea75EffectSlots + slot) =
        static_cast<unsigned char>(static_cast<std::int32_t>(AddressOf(s) - AddressOf(Effect_Objects)) >> 7);
    s[0xA] = 0x80;
    SetUL(S() + 0xC, 0);
    S()[6] = 0x1E;
    S()[0x5D] = 0x80;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47EFB0 (state 1): +0xA plus the byte +0xC; the dword +0xC = 0; the
// beam of length +6, blend 0.
extern "C" void __cdecl EffectKind5B_Glow(void) {
    unsigned char* const s = S();
    s[0xA] = static_cast<unsigned char>(s[0xA] + s[0xC]);
    SetUL(S() + 0xC, 0);
    SH_CALL(EffectKind5B_DrawBeam)(S()[6], 0);
}

// original 0x47EFF0 (state 2; PSX twin 0x801F2DD8 in AREA075): +0x5D down 2; at
// 0 Effect_Release (and on); +0x5D's low five bits 0: +6 up. The beam of length
// +6, blend 1.
extern "C" void __cdecl EffectKind5B_Fade(void) {
    unsigned char* s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
    s = S();
    if (s[0x5D] == 0) {
        SH_CALL(Effect_Release)();
        s = S();
    }
    if ((s[0x5D] & 0x1F) == 0) {
        s[6] = static_cast<unsigned char>(s[6] + 1);
        s = S();
    }
    SH_CALL(EffectKind5B_DrawBeam)(s[6], 1);
}

// original 0x47F040 (called by states 1 and 2; cdecl; PSX twin 0x801F2E64 in
// AREA075): a texture window (0xD0, 0x40, 0x10, 0x10) built at Gfx_PacketNext
// and linked at the record's (x, z) (0xC); a POLY_FT4 whose corners are the
// point (x, z -/+ 0x8000, +0x3C + 0xE00000) and the ground (AreaMap_Elevation
// at x - d) at x - d, d = (length & 0xFFFF) << 15, projected by
// EffectGte_ProjectPoint (Sprite_Current read again for each); CLUT (0xD0,
// 0x1E3), page (0, 1, 0x2C0, 0x100), u from +0xA & 7, shade +0x5D, blend
// abr & 0xFF - linked (0x48); the texture window put back (0, 0, 0x100, 0x100),
// linked (0xC).
extern "C" void __cdecl EffectKind5B_DrawBeam(unsigned length, unsigned abr) {
    using Rect = void (__cdecl*)(unsigned char*, const unsigned char*);
    unsigned char* rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, 0xD0);
    SetWord(rect + 2, 0x40);
    SetWord(rect + 4, 0x10);
    SetWord(rect + 6, 0x10);
    SH_AT(Rect, at::kPrimFromRect)(Gfx_PacketNext, rect);
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    const U d = (length & 0xFFFF) << 15;
    s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34) - d), static_cast<long>(UL(s + 0x38)));
    unsigned char* const p = Gfx_PacketNext;
    const U low = (static_cast<U>(S16(static_cast<U>(ground))) << 16) + 0xE00000u;
    SH_CALL(Gpu_SetPolyFT4)(p);
    long point[3];
    s = S();
    point[0] = static_cast<long>(UL(s + 0x34) - d);
    point[1] = static_cast<long>(UL(s + 0x38) - 0x8000u);
    point[2] = static_cast<long>(low);
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 8));
    s = S();
    point[0] = static_cast<long>(UL(s + 0x34));
    point[1] = static_cast<long>(UL(s + 0x38) - 0x8000u);
    point[2] = static_cast<long>(UL(s + 0x3C) + 0xE00000u);
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 0x18));
    s = S();
    point[0] = static_cast<long>(UL(s + 0x34) - d);
    point[1] = static_cast<long>(UL(s + 0x38) + 0x8000u);
    point[2] = static_cast<long>(low);
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 0x28));
    s = S();
    point[0] = static_cast<long>(UL(s + 0x34));
    point[1] = static_cast<long>(UL(s + 0x38) + 0x8000u);
    point[2] = static_cast<long>(UL(s + 0x3C) + 0xE00000u);
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 0x38));
    SetWord(p + 0x16, SH_CALL(Gpu_GetClut)(0xD0, 0x1E3));
    SetWord(p + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    s = S();
    const auto u = static_cast<unsigned char>(s[0xA] & 7);
    p[0x15] = 0;
    p[0x14] = u;
    p[0x25] = 0;
    p[0x24] = static_cast<unsigned char>(u + 0x80);
    p[0x35] = 0x10;
    p[0x34] = u;
    p[0x45] = 0x10;
    p[0x44] = static_cast<unsigned char>(u + 0x80);
    p[4] = s[0x5D];
    p[5] = s[0x5D];
    p[6] = s[0x5D];
    SH_CALL(Gpu_SetSemiTrans)(p, abr & 0xFF);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x48);
    rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, 0);
    SetWord(rect + 2, 0);
    SetWord(rect + 4, 0x100);
    SetWord(rect + 6, 0x100);
    SH_AT(Rect, at::kPrimFromRect)(Gfx_PacketNext, rect);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
}

// ===========================================================================
// Kinds 0x5D, 0x5E, 0x5F: dispatchers only
// ===========================================================================

// original 0x47F2B0 (Effect_KindHandlers[0x5D], hidden in 0x47F040):
// jmp [EffectKind5D_States + +1 * 4], unbounded. Its states are catalog part 6
// rows (0x47F2D0..), no group's this round.
extern "C" void __cdecl EffectKind5D_Run(void) {
    Dispatch("EffectKind5D_Run", AddressOf(EffectKind5D_States), EffectKind5D_States_count, 1);
}

// original 0x47F5D0 (Effect_KindHandlers[0x5E]; a catalog part 6 row the cut
// does not list, taken as the band's dispatcher): jmp [EffectKind5E_States +
// +1 * 4], unbounded.
extern "C" void __cdecl EffectKind5E_Run(void) {
    Dispatch("EffectKind5E_Run", AddressOf(EffectKind5E_States), EffectKind5E_States_count, 1);
}

// original 0x47FD80 (Effect_KindHandlers[0x5F], hidden in 0x47FBE0):
// jmp [EffectKind5F_States + +1 * 4], unbounded. Its state 0 is
// Task_StartHold60 (magic_engine.cpp); the rest are part 6 rows and E3A's.
extern "C" void __cdecl EffectKind5F_Run(void) {
    Dispatch("EffectKind5F_Run", AddressOf(EffectKind5F_States), EffectKind5F_States_count, 1);
}

void Effect2G_Inject() {
    if (bof3::WantsShadow("effect_2g")) effect_2g::SelfTest();
    BOF3_INJECT(EffectKind66_Run);
    BOF3_INJECT(EffectKind66_Start);
    BOF3_INJECT(EffectKind66_Open);
    BOF3_INJECT(EffectKind66_Show2);
    BOF3_INJECT(EffectKind66_Show3);
    BOF3_INJECT(EffectKind66_Show4);
    BOF3_INJECT(EffectKind66_Close);
    BOF3_INJECT(EffectKind66_Message);
    BOF3_INJECT(EffectKind66_DrawPiece);
    BOF3_INJECT(EffectKind18Sub20_Run);
    BOF3_INJECT(EffectKind18Sub20_Start);
    BOF3_INJECT(EffectKind18Sub20_WaitSet);
    BOF3_INJECT(EffectKind18Sub20_Lower);
    BOF3_INJECT(EffectKind18Sub20_WaitClear);
    BOF3_INJECT(EffectKind18Sub20_Raise);
    BOF3_INJECT(EffectKind18Sub20_Draw);
    BOF3_INJECT(EffectKind15_Run);
    BOF3_INJECT(EffectKind15_Start);
    BOF3_INJECT(EffectKind15_Begin);
    BOF3_INJECT(EffectKind15_WaitPose4);
    BOF3_INJECT(EffectKind15_Countdown);
    BOF3_INJECT(EffectKind15_WaitCue3);
    BOF3_INJECT(EffectKind15_Cooldown);
    BOF3_INJECT(EffectKind15_Hit);
    BOF3_INJECT(EffectKind15_WaitPose8);
    BOF3_INJECT(EffectKind15_End);
    BOF3_INJECT(EffectKind54_Run);
    BOF3_INJECT(EffectKind54_Start);
    BOF3_INJECT(EffectKind54_Arm);
    BOF3_INJECT(EffectKind54_Cue);
    BOF3_INJECT(EffectKind54_Strike);
    BOF3_INJECT(EffectKind54_WaitPose2);
    BOF3_INJECT(EffectKind54_WaitPose8);
    BOF3_INJECT(EffectKind54_Rearm);
    BOF3_INJECT(EffectKind54_Recoil);
    BOF3_INJECT(EffectKind54_WaitPose2B);
    BOF3_INJECT(EffectKind54_End);
    BOF3_INJECT(EffectKind55_Run);
    BOF3_INJECT(EffectKind55_Start);
    BOF3_INJECT(EffectKind55_Tick);
    BOF3_INJECT(EffectKind55_Finish);
    BOF3_INJECT(EffectKind55_DrawTime);
    BOF3_INJECT(EffectKind55_DrawCount);
    BOF3_INJECT(EffectKind57_Run);
    BOF3_INJECT(EffectKind57_Show);
    BOF3_INJECT(EffectKind57_Release);
    BOF3_INJECT(EffectKind57_DrawRing);
    BOF3_INJECT(EffectKind5A_Run);
    BOF3_INJECT(EffectKind5A_Place);
    BOF3_INJECT(EffectKind5A_Draw);
    BOF3_INJECT(EffectKind5B_Run);
    BOF3_INJECT(EffectKind5B_Start);
    BOF3_INJECT(EffectKind5B_Glow);
    BOF3_INJECT(EffectKind5B_Fade);
    BOF3_INJECT(EffectKind5B_DrawBeam);
    BOF3_INJECT(EffectKind5D_Run);
    BOF3_INJECT(EffectKind5E_Run);
    BOF3_INJECT(EffectKind5F_Run);
}

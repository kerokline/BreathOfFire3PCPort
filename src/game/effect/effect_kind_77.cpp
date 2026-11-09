// Moved here by tools/move_funcs.py from effect_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_77.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }

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

constexpr U kFloat320 = 0x43A00000u;   // 320.0f

// The full-screen quad of kinds 0x76, 0x79, 0x7A, 0x7B and 0x7C: a draw mode
// (page Gpu_GetTPage(2, abr, 0x140, 0x140), dtd 1) committed (6, 0xC); a POLY_G4
// over (0, 0) - (320, 320), its four vertices' red `shade` (read from `cell`
// for each vertex when given) and green and blue 0, semi-transparent,
// committed (2, 0x44). The depth words are not written.
void FullScreen(unsigned abr, const unsigned char* cell) {
    const U page = SH_CALL(Gpu_GetTPage)(2, abr, 0x140, 0x140);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    // DIV-0041: the left corners at Widescreen_FillX(), the right at 320 + the
    // columns (EffectKind96_Pulse's rule); (0, 320) narrow or unarmed. The y
    // corners stay Capcom's 0 and 320.
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillRight(320.0f));   // kFloat320 narrow
    SetBits(p + 8, left);
    SetBits(p + 0xC, 0);
    SetBits(p + 0x18, right);
    SetBits(p + 0x1C, 0);
    SetBits(p + 0x28, left);
    SetBits(p + 0x2C, kFloat320);
    SetBits(p + 0x38, right);
    SetBits(p + 0x3C, kFloat320);
    p[0x14] = cell ? *cell : 0;
    p[4] = cell ? *cell : 0;
    p[0x15] = 0;
    p[5] = 0;
    p[0x16] = 0;
    p[6] = 0;
    p[0x34] = cell ? *cell : 0;
    p[0x24] = cell ? *cell : 0;
    p[0x35] = 0;
    p[0x25] = 0;
    p[0x36] = 0;
    p[0x26] = 0;
    SH_CALL(Gpu_SetSemiTrans)(Gfx_PacketNext, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
}

// Kinds 0x7B / 0x7C state 1: +9 up; past 0x10 the shade is the next of the two
// (0x654AF8 by 0x676279, which alternates 0, 1) and +9 0. Then the quad.
void Pulse(const char* who) {
    unsigned char* const s = S();
    const unsigned frames = s[9];
    s[9] = static_cast<unsigned char>(frames + 1);
    if (frames >= 0x10) {
        const unsigned index = Effect3d_PulseIndex;
        if (index >= at::kPulseShadeCount)
            bof3::Fatal("%s: 0x676279 is %u, past the two shades at 0x654AF8 - the original reads the bytes after "
                        "(docs/effect_3d.md section 7)",
                        who, index);
        Effect3d_PulseShade = B(::bof3::Img(at::kPulseShades.v) + index);
        S()[9] = 0;
        const unsigned old = Effect3d_PulseIndex;
        Effect3d_PulseIndex = static_cast<unsigned char>(old + 1);
        if (old >= 1) Effect3d_PulseIndex = 0;
    }
    FullScreen(2, At(::bof3::Img(Effect3d_PulseShade_addr)));
}

// Kinds 0x7B / 0x7C state 0: +9 0, +1 1, the shade 0xF, the index 0.
void PulseStart() {
    S()[9] = 0;
    S()[1] = 1;
    Effect3d_PulseShade = 0xF;
    Effect3d_PulseIndex = 0;
}

}  // namespace

// original 0x485CB0 (Effect_KindHandlers[0x76], hidden in E3C's 0x485960): the
// full-screen quad, colour 0, abr 2.
extern "C" void __cdecl EffectKind76_Shade(void) { FullScreen(2, nullptr); }

// original 0x485D60 (Effect_KindHandlers[0x79]): the same; then, when the
// counter 0x903848 is 1, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind79_Shade(void) {
    FullScreen(2, nullptr);
    if (B(::bof3::Img(at::kCounter.v)) == 1) SH_CALL(Effect_Release)();
}

// original 0x485E10 (Effect_KindHandlers[0x7A]): the quad with abr 0.
extern "C" void __cdecl EffectKind7A_Shade(void) { FullScreen(0, nullptr); }

// original 0x485EC0 (Effect_KindHandlers[0x7B]): jmp [EffectKind7B_States + +1 * 4].
extern "C" void __cdecl EffectKind7B_Run(void) {
    Dispatch("EffectKind7B_Run", AddressOf(EffectKind7B_States), EffectKind7B_States_count, 1);
}

// original 0x485EE0 (state 0).
extern "C" void __cdecl EffectKind7B_Start(void) { PulseStart(); }

// original 0x485F10 (state 1): the pulse, the quad; no end of its own.
extern "C" void __cdecl EffectKind7B_Pulse(void) { Pulse("EffectKind7B_Pulse"); }

// original 0x486020 (Effect_KindHandlers[0x7C]): jmp [EffectKind7C_States + +1 * 4].
extern "C" void __cdecl EffectKind7C_Run(void) {
    Dispatch("EffectKind7C_Run", AddressOf(EffectKind7C_States), EffectKind7C_States_count, 1);
}

// original 0x486040 (state 0): byte for byte 0x485EE0.
extern "C" void __cdecl EffectKind7C_Start(void) { PulseStart(); }

// original 0x486070 (state 1): the pulse, the quad; then, when the counter
// 0x903848 is 4, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind7C_Pulse(void) {
    Pulse("EffectKind7C_Pulse");
    if (B(::bof3::Img(at::kCounter.v)) == 4) SH_CALL(Effect_Release)();
}

// original 0x486180 (Effect_KindHandlers[0x77]): jmp [EffectKind77_States + +1 * 4].
extern "C" void __cdecl EffectKind77_Run(void) {
    Dispatch("EffectKind77_Run", AddressOf(EffectKind77_States), EffectKind77_States_count, 1);
}

// original 0x4861A0 (state 0): jmp [EffectKind77_CountSteps + +2 * 4].
extern "C" void __cdecl EffectKind77_Count(void) {
    Dispatch("EffectKind77_Count", AddressOf(EffectKind77_CountSteps), EffectKind77_CountSteps_count, 2);
}

// original 0x4861C0 (state 0's step 0): the tally word 0x939A00 = Rand() << 15
// (count 0, frames 0, the pace bit Rand's bit 0); +2 up.
extern "C" void __cdecl EffectKind77_Begin(void) {
    const U r = static_cast<U>(SH_CALL(Rand)());
    SetWord(At(::bof3::Img(at::kTally.v)), (r << 15) & 0xFFFFu);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4861E0 (step 1 of both, PSX twin 0x801F7240): Effect_Release when
// the counter 0x903848 is 1. Else a count of 200 (0xC8) or more: +2 up.
// Else the frames (bits 0..6 of the high byte) up one; at 30 (pace 0) or 40
// (pace 1) they are 0 and the count up, Sound_PlayEffect(0x20A) when the
// chapter row's flag 0x14 is set. The pace bit kept; a frame count of 0x80
// (from 0x7F) carries into it.
extern "C" void __cdecl EffectKind77_Tick(void) {
    if (B(::bof3::Img(at::kCounter.v)) == 1) {
        SH_CALL(Effect_Release)();
        return;
    }
    const U word = W(::bof3::Img(at::kTally.v));
    unsigned count = word & 0xFFu;
    if (count >= 0xC8) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        return;
    }
    const unsigned pace = (word >> 15) & 1u;
    unsigned frames = (((word >> 8) & 0x7Fu) + 1u) & 0xFFu;
    if (pace == 0 ? frames >= 0x1E : frames >= 0x28) {
        count = (count + 1u) & 0xFFu;
        if (SH_CALL(Flags_Test)(P(Chapter_FlagRow), at::kTallySoundFlag)) SH_CALL(Sound_PlayEffect)(0x20A);
        frames = 0;
    }
    const unsigned high = ((pace << 7) | frames) & 0xFFu;
    SetWord(At(::bof3::Img(at::kTally.v)), (high << 8) | count);
}

// original 0x486280 (state 1, a start no list of the cut has): jmp
// [EffectKind77_ResumeSteps + +2 * 4] - the count without its reset.
extern "C" void __cdecl EffectKind77_Resume(void) {
    Dispatch("EffectKind77_Resume", AddressOf(EffectKind77_ResumeSteps), EffectKind77_ResumeSteps_count, 2);
}

// original 0x4862A0 (step 2 of state 0, step 1 of state 1): nothing while a
// message is open (Field_Request 2), in game mode 4, or while the chapter
// row's flag 0x2E is set; else 0x9039A3 bit 0, the step 0x19, run 5, and a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind77_End(void) {
    if (Field_Request == 2) return;
    if (Game_Mode == 4) return;
    if (SH_CALL(Flags_Test)(P(Chapter_FlagRow), at::kTallyEndFlag)) return;
    B(::bof3::Img(at::kScriptFlagsHigh.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kScriptFlagsHigh.v)) | 1u);
    MoveScript_PartyRecords[0x65] = 0x19;
    MoveScript_Var7 = 5;
    SH_CALL(Effect_Release)();
}

// original 0x4862F0 (state 2): the box and the count; then Effect_Release
// unless a message is open or the game mode is 4.
extern "C" void __cdecl EffectKind77_Show(void) {
    SH_CALL(EffectKind77_DrawCount)();
    if (Field_Request == 2) return;
    if (Game_Mode == 4) return;
    SH_CALL(Effect_Release)();
}

// original 0x486310: a menu box (0x586160(0x8A, 0x1E, 0x2D, 0x14, 1)), the
// count (0x939A00's low byte) printed by the format 0x653074 into 0x904BA0 and
// drawn by Text_DrawFont12 at (0x8F, 0x21), colour 0.
extern "C" void __cdecl EffectKind77_DrawCount(void) {
    SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBox.v)(0x8A, 0x1E, 0x2D, 0x14, 1);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(P(::bof3::Img(at::kText.v))), reinterpret_cast<const char*>(P(::bof3::Img(at::kTallyFormat.v))),
                         static_cast<int>(B(::bof3::Img(at::kTally.v))));
    SH_CALL(Text_DrawFont12)(0x8F, 0x21, 0, P(::bof3::Img(at::kText.v)));
}

void EffectKind77_Inject() {
    BOF3_INJECT(EffectKind76_Shade);
    BOF3_INJECT(EffectKind79_Shade);
    BOF3_INJECT(EffectKind7A_Shade);
    BOF3_INJECT(EffectKind7B_Run);
    BOF3_INJECT(EffectKind7B_Start);
    BOF3_INJECT(EffectKind7B_Pulse);
    BOF3_INJECT(EffectKind7C_Run);
    BOF3_INJECT(EffectKind7C_Start);
    BOF3_INJECT(EffectKind7C_Pulse);
    BOF3_INJECT(EffectKind77_Run);
    BOF3_INJECT(EffectKind77_Count);
    BOF3_INJECT(EffectKind77_Begin);
    BOF3_INJECT(EffectKind77_Tick);
    BOF3_INJECT(EffectKind77_Resume);
    BOF3_INJECT(EffectKind77_End);
    BOF3_INJECT(EffectKind77_Show);
    BOF3_INJECT(EffectKind77_DrawCount);
}

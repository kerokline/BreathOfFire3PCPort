// Six spell overlays compiled into the exe, round nine group C1
// (docs/magic_c1.md): the unfinished skills TCRF lists, as the PC's code has
// them (docs/cut-content.md section 2). The names are this project's, from
// what the code does; TCRF's names are third-party labels for the PlayStation
// release and are only cited in the doc.
//
//   - MAGIC010 0x49DEF0..0x49DFF4 (row 9, TCRF's White Flag): the source sprite
//     tinted and faded back, the target actor flashed;
//   - MAGIC080 0x4FC330..0x4FC696 (row 27, shared by many ids): a row of
//     glyphs drawn over a point for 0x30 frames - one time in 64 a row of six
//     characters from the exe's strings instead;
//   - MAGIC113 0x4D67F0..0x4D7952 (row 10, TCRF's Pentagram): a disc, two
//     rings, a five-line figure, a band of quads and six sprites, in turn;
//   - MAGIC145 0x4E9A70..0x4E9EE0 (row 149, TCRF's Ink) and MAGIC146
//     0x4E9EF0..0x4EA446 (row 150, Ink Ink): textured puffs over the source
//     sprite (145) or over every living actor of the target's side (146);
//   - MAGIC213 0x4F4A60..0x4F50A6 (row 148): each living actor of a side
//     tinted up and down, seven sprite motes circling it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table (a stack table, or a .data table read in place) aborts where
// the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_c1.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Word;

// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Other groups' functions and the phase handlers a stack table holds are ours
// now and called by name (bof3::addr): Port_DroppedCall (MAGIC124's extent, a
// bare ret) and S37's MagicFx_FreeCurrentRecord among them.

// The sprite animation bank pointer Sprite_SetAnimationAt reads (0x8B3580 the
// area's, 0x8C5D80 the battle's, 0x8E3580 the spell's).
// kFrameSet: now BattleE2_AnimSet (symbols.toml).

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC010 (row 9)

// ===========================================================================
// MAGIC080 (row 27)

// original 0x4FC330: the kind-2 task. A two-entry stack table by +1:
// Magic080_Start, Magic080_Run.
C1_EXPORT void __cdecl Magic080_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic080_Start, bof3::addr::Magic080_Run};
    StackCall(kPhases, 2, Sc()[1], "Magic080_Task");
}

// original 0x4FC360: the task at (Field_Kind2X, Field_Kind2Z) and the map's
// elevation there; +0xB Rand & 0x3F (0 picks the text); +9 0x30; +1 on.
C1_EXPORT void __cdecl Magic080_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(Field_Kind2X));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(Field_Kind2Z));
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const long h = MH_CALL(AreaMap_Elevation)(ts->x, ts->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(h));
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0x3F);
    Sc()[9] = 0x30;
    Inc(Sc()[1]);
}

// original 0x4FC3C0: the screen point, its x 0x24 to the left; the glyph row
// (+0xB not 0) or the text (0); +9 down, at 0 the target's flag 0x40, the
// done flag, the task freed.
C1_EXPORT void __cdecl Magic080_Run(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) - 0x24u);
    if (Sc()[0xB] != 0)
        Call0(bof3::addr::Magic080_DrawGlyphs);
    else
        Call0(bof3::addr::Magic080_DrawText);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FC420: six characters, one from the head of each string the
// six pointers at 0x66A3C8 name (its first byte & 0x7F, its second: the
// character's code at +0x16), each a code-0x6C primitive 12 wide and 12 high
// from the screen point (+0x2E, +0x30) on, grey 0x80, CLUT (0, 0x1E0),
// committed to slot 2.
C1_EXPORT void __cdecl Magic080_DrawText(void) {
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetCode6C)(p);
        const unsigned x = i * 12;
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x2, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y2, Word(Sc() + 0x30) + 12u);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x3, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y3, Word(Sc() + 0x30) + 12u);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        const unsigned char* const str = Magic080_TextPointers[i];
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->glyph, ((str[0] & 0x7Fu) << 8) + str[1]);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        p[0xC] = 0;
        p[0xD] = 0;
        p[0x14] = 12;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = 12;
        p[0x24] = 12;
        p[0x25] = 12;
        MH_CALL(Gfx_CommitPrim)(2, 0x28);
    }
}

// original 0x4FC540: seven glyphs, textured quads 12 wide and 12 high from
// the screen point on, grey 0x80, page (0x3C0, 0) with blend 1, CLUT (0,
// 0x1E0), each cell's u / v the byte pair at 0x65DA28 + 4 i / + 2; slot 2.
C1_EXPORT void __cdecl Magic080_DrawGlyphs(void) {
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        const int x0 = static_cast<int>(i * 12), x1 = x0 + 12;
        PutFloat(p + 8, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutFloat(p + 0x18, S16(Sc() + 0x2E) + x1);
        PutFloat(p + 0x1C, S16(Sc() + 0x30));
        PutFloat(p + 0x28, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0x2C, S16(Sc() + 0x30) + 12);
        PutFloat(p + 0x38, S16(Sc() + 0x2E) + x1);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        PutFloat(p + 0x3C, S16(Sc() + 0x30) + 12);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        const unsigned char* const uv = Magic080_GlyphCells + 4 * i;
        p[0x14] = uv[0];
        p[0x15] = uv[2];
        p[0x24] = static_cast<unsigned char>(uv[0] + 12);
        p[0x25] = uv[2];
        p[0x34] = uv[0];
        p[0x35] = static_cast<unsigned char>(uv[2] + 12);
        p[0x44] = static_cast<unsigned char>(uv[0] + 12);
        p[0x45] = static_cast<unsigned char>(uv[2] + 12);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
}

// ===========================================================================
// MAGIC113 (row 10)

// ===========================================================================
// MAGIC145 (row 149) and MAGIC146 (row 150)

namespace {

// kInkPool: now InkInkPuff_Pool (symbols.toml). InkInkPuff_Pool
// kMotePool: now Magic213Mote_Pool (symbols.toml). Magic213Mote_Pool

}  // namespace

// ===========================================================================
// MAGIC213 (row 148)

void MagicC1_Inject() {
    if (bof3::WantsShadow("magic_c1")) magic_c1::SelfTest();
    BOF3_INJECT(Magic080_Task);
    BOF3_INJECT(Magic080_Start);
    BOF3_INJECT(Magic080_Run);
    BOF3_INJECT(Magic080_DrawText);
    BOF3_INJECT(Magic080_DrawGlyphs);
}

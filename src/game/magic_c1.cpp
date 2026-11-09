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
}

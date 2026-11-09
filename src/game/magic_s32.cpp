// Two spell overlays compiled into the exe, round nine group S32
// (docs/magic_s32.md): the PSX's MAGIC144 and MAGIC150.EMI, Magic_Rows rows
// 67 and 72. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Wall of Fire and Eye Beam; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC144 0x4E9140..0x4E9A66: a kind-2 task that walks a pool of its own
//     (48 records of 0x84 at 0x6A5680) and one kind-1 child (parameter 0x36)
//     at the source sprite: the child spawns three pool motes (a flame and two
//     sparks), darkens the source and flags the target, and draws a glowing
//     disc under the actor's matrix. The flame draws with MAGIC092's
//     Magic092_DrawFlame (group S20); MAGIC092 in turn runs this overlay's
//     flame and spark steps, the disc and the spark draw by address - the
//     two overlays are one effect's two variants;
//   - MAGIC150 0x4EA450..0x4EAE61: a seven-step task over a state block of its
//     own (0x6A9040..0x6A9107): eight sparks gather at a point in front of the
//     caster, then a beam - a ring of flat quads round the x axis and a
//     spiral ribbon of 0x80 gouraud quads jittered by Rand - widens, fires
//     (the target flagged 0x40) and narrows.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful
// replacement, except that a phase past one of the two stack tables or the
// five .data tables aborts where the original would call through whatever
// follows it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s32.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// kV: now Prim_VertexScratch (symbols.toml).

// MAGIC144's .data dispatch tables (symbols.toml, read in place).
// kChildTaskTable: now WallOfFireChild_TaskTable (symbols.toml). by +1: one entry
// kChildSteps: now WallOfFireChild_Steps (symbols.toml). by +2: four
// kMoteKinds: now WallOfFireMote_Kinds (symbols.toml). by +1: flame, spark
// kFlameSteps: now WallOfFireFlame_Steps (symbols.toml). by +2: four
// kSparkSteps: now WallOfFireSpark_Steps (symbols.toml). by +2: four

unsigned char* Sc() { return Sprite_Current; }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

int S16MagicS32(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, (Word(p) + v) & 0xFFFF); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

int RandCall() { return MH_CALL(Rand)(); }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions and Capcom's called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using PtrFn = void (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define S32_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC144 (row 67, Wall of Fire read one id down)

// ===========================================================================
// MAGIC150 (row 72, Eye Beam read one id down)

void MagicS32_Inject() {
    if (bof3::WantsShadow("magic_s32")) magic_s32::SelfTest();
}

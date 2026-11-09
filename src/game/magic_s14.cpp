// Three spell overlays compiled into the exe, round nine group S14
// (docs/magic_s14.md): the PSX's MAGIC064, MAGIC065 and MAGIC066.EMI,
// Magic_Rows rows 77, 70 and 17. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Weretiger, Pilfer and Tsunami; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC064 0x4B3F00..0x4B54A8: the acting party member posed, the field
//     view re-centred on it (0x4B5350 rebuilds the map view round the kind-2
//     point), a burst of line streaks from a pool of 0xC0 records (0x683288,
//     the current one at 0x684788), a copy of the member that spawns four
//     textured images of it and a backdrop dim; its form's DAT file loaded
//     while two sprite pairs cross-fade, its palette and CLUT redone, the view
//     put back;
//   - MAGIC065: one function, 0x4B58F0, the item-name copy into Text_Records
//     that Pilfer's and Steal's successful rolls call (Pilfer's other eight
//     functions are round eight's, magic_fx_reached.cpp);
//   - MAGIC066 0x4B59A0..0x4B66C0: a wave of 127 textured quads and two
//     gouraud ones up a sine across the field, then rings of four textured
//     quads round every live enemy and party member at three heights.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s14.h"

#include "game/draw_pool.h"
#include "game/map_rim.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

// kPoolCurrent: now WeretigerStreak_Current (symbols.toml).
// kRingPhases: now TsunamiRing_Phases (symbols.toml).

// Overlay .data read in place (addresses only; the values are the exe's).
// kStreakOffsets: now WeretigerStreak_Offsets (symbols.toml). 2 signed bytes (x, y) a facing
// kImageUV: now WeretigerImage_UV (symbols.toml). 4 bytes an image: u at +0, v at +2
// kRingOffsets: now TsunamiRing_Offsets (symbols.toml). 3 of (x, z, y) dwords

// kFrameCounter: now Frame_Counter (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return Low32(p); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The party records by the battle index, unchecked (the originals index
// ObjTrio by the actor byte whatever it holds).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

#define S14_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

}  // namespace

#define S14_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC064 (row 77, Weretiger read one id down)

// ===========================================================================
// MAGIC065 (row 70, Pilfer read one id down)

// ===========================================================================
// MAGIC066 (row 17, Tsunami read one id down)

namespace {

// The ring's matrix block as the original lays it out on its stack: MATRIX at
// +0x14 of the frame, RotTrans writing its translation at the MATRIX's +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

void MagicS14_Inject() {
    if (bof3::WantsShadow("magic_s14")) magic_s14::SelfTest();
}

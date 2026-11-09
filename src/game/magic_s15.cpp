// Three spell overlays compiled into the exe, round nine group S15
// (docs/magic_s15.md): the PSX's MAGIC067, MAGIC068 and MAGIC069.EMI,
// Magic_Rows rows 18, 40 and 75. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Chill, Foretell and Influence; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC067 0x4B66D0..0x4B73F6: Corona's shape (group S31, MAGIC137) - a
//     ray and a full-screen flash (kind 1, 0x13), and in an event battle a
//     copy of the acting enemy - then, once both have ended, one marker child
//     at each actor of the target's side that is not out;
//   - MAGIC068 0x4B7400..0x4B7D39: one child (kind 1, 0x1C) that shows a
//     counting texture cell above the caster; then a message chosen from the
//     two sides' records (a score from each member's ratio of two record
//     words, the sides' mean of a third, and a table), six flag bits from the
//     live enemies' record bytes drawn as tiles until the message closes;
//   - MAGIC069 0x4B7DE0..0x4B8D66: six children (kind 1, 0x39) that slide in
//     from either side of the caster's screen point drawing a ring, triangles
//     and a cross; the first then spawns a child at every actor (not the
//     actor or the target) whose record words or flag bits pass a test, each
//     tinting its actor's sprite in and out under a textured quad.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), the ray matrix
// aborts on a facing past 3, where the original turns by an uninitialised
// stack word (group S31's CoronaRay_PushMatrix, the same code), and
// Foretell_Read aborts on a zero divisor, where the original's idiv faults
// (the live-target divide, docs/takeover-queue-round9.md section 6).
#include "game/magic_s15.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// The cells the overlays read beyond the harness's names.
// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle
// kBattleB2: now Battle_EnemyCount (symbols.toml). u8: a battle byte Foretell_Read adds in (not read further)

// The overlays' own .data (read in place, as the originals read it).
// kForetellBias: now Foretell_Bias[7] (symbols.toml). Foretell_Bias + 7: s8, by party count - enemy count
// kForetellTiles: now Foretell_TileColours (symbols.toml). Foretell_TileColours: six entries of three words
// kInfluencePoints: now Influence_TrianglePoints (symbols.toml). Influence_TrianglePoints: four u8 angles

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned ActorIndex() { return static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
bool IsOut(unsigned actor) { return (MH_CALL(Battle_ActorIsOut)(actor & 0xFF) & 0xFFu) != 0; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// The actor-matrix push of 0x4B6BC0 (group S31's 0x4E77D0 shape):
// Camera_Matrix x the task's, turned about z, translation RotTrans of
// (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX block as the
// original lays it out on its stack, RotTrans writing its translation at
// +0x14; the task is read once, after the push.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

#define S15_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC067 (row 18, Chill read one id down)

// ===========================================================================
// MAGIC068 (row 40, Foretell read one id down)

// ===========================================================================
// MAGIC069 (row 75, Influence read one id down)

void MagicS15_Inject() {
    if (bof3::WantsShadow("magic_s15")) magic_s15::SelfTest();
}

// Spell group S24 of round nine (docs/magic_s24.md): three BMAGIC overlays of
// Magic_Rows compiled into the exe, 47 functions.
//
//   MAGIC104, row 14 (read one id down: Sirocco): a kind-2 task that rings the
//     targets with eight whirls twice over - each a kind-1 child drawing a
//     funnel of POLY_G4, a disc of POLY_G3 and a ring of POLY_G4 - then a
//     second ring of shrinking spheres, and ends with the targets' 0x40 flag.
//   MAGIC105, row 64 (read one id down: Kyrie): a kind-2 task with seven orbs
//     (kind-1 children, drawn as stacked POLY_G4 bands) that shed motes into a
//     64-record pool of its own; each mote is a sprite that drifts and sinks.
//   MAGIC106, row 104 (read one id down: Death): a kind-2 task with a pool of
//     96 sparks that close in on the owner, spin, rise and fade, each drawn as
//     four POLY_G3 shards.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a direct call to another function of the group goes by its address too
// (Phase), which in the game is the jmp Inject put there to ours.
//
// No divergence: each function is a faithful replacement, except that a
// dispatch past its table - a stack table's or a .data table's - aborts where
// the original would call through whatever follows (docs/magic_fx_reached.md
// section 3, the precedent). Where the original reads memory again after a
// call (Sprite_Current, the owner, the pools' current spark, the scratch
// words), ours reads it again.
#include "game/magic_s24.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s24_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace cell = magic_s24::cell;
namespace tbl = magic_s24::tbl;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- small helpers ---------------------------------------------------------

// The original's imul: 32 bits, wrapping.
inline int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
inline int Shl(int a, int n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n); }
inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
inline short G16(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }
inline void Put16(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v) & 0xFFFFu); }
inline std::uint32_t Addr(const void* p) { return Low32(p); }
inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(Addr(p))); }
inline void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
inline void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
inline unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
using TurnFn = void (__cdecl*)(unsigned char*);
using AllocFn = unsigned char (__cdecl*)();
void CentreOnTargets() { MH_CALL(MagicFx_CenterOnSide)(); }
void TurnByFacing(unsigned char* task) { MH_AT(TurnFn, magic_s24::kTurnByFacing)(task); }
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The scratch words (SharedScratch): radii at +0 / +2, then colours or angles.
constexpr ::bof3::image::Va kR{cell::kScratch};
constexpr ::bof3::image::Va kR2{cell::kScratch + 2};
constexpr ::bof3::image::Va kW4{cell::kScratch + 4};
constexpr ::bof3::image::Va kW6{cell::kScratch + 6};
constexpr ::bof3::image::Va kW8{cell::kScratch + 8};
// kWC: now Scratch_Swap (symbols.toml).
constexpr ::bof3::image::Va kWA{cell::kScratch + 0xA};
constexpr ::bof3::image::Va kWE{cell::kScratch + 0xE};
// The four SVECTORs of Prim_VertexScratch, as the originals name them: a (+0), b (+8), c (+0x10), d (+0x18).
// kA: now Prim_VertexScratch (symbols.toml).
// kB: now Prim_VertexScratch[1] (symbols.toml).
// kC: now Prim_VertexScratch[2] (symbols.toml).
// kD: now Prim_VertexScratch[3] (symbols.toml).

const short* Vec(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }
float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// The four-vertex projection every POLY_G4 of the group makes: a b c d into
// the primitive's four points, then its depth.
void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}
void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10B)(p);
}

// The matrix every draw of the group pushes: the angles rotated, times the
// camera, translated by the effect's point.
struct Matrix { short m[3][3]; short pad; std::int32_t t[3]; };
void PushMatrix(const short vector[3], const short angles[3]) {
    Matrix m;
    MH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(m.t));
    MH_CALL(Gte_RotMatrix)(angles, &m.m[0][0]);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, &m.m[0][0], &m.m[0][0]);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}
// The effect point as the GTE takes it: (x >> 9) - 0x4000, (z >> 9) - 0x4000,
// minus half the height (rounded toward 0).
void PointOf(short v[3], std::int32_t x, std::int32_t z, short height) {
    v[0] = static_cast<short>((x >> 9) - 0x4000);
    v[1] = static_cast<short>((z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(height / 2));
}

}  // namespace

#define S24_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC104 (row 14)

// ===========================================================================
// MAGIC105 (row 64)

// ===========================================================================
// MAGIC106 (row 104)

void MagicS24_Inject() {
    if (bof3::WantsShadow("magic_s24")) magic_s24::SelfTest();
}

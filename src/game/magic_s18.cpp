// Two overlays of Magic_Rows, round nine group S18 (docs/magic_s18.md):
//
//   - MAGIC079.EMI, row 52 (the ability ids 0x4F and 0xB7; read one id down,
//     Drain): 0x4BEB50..0x4BF8C3, 25 functions. A kind-2 task that starts
//     four kind-1 children (parameter 0x2B), each of one of four types by its
//     +1, each type a .data phase table and one of two draws.
//   - MAGIC082.EMI, row 32 (the ids 0x22 0x23 0x52 0x54 0x55 0xB8 0xBA 0xBB;
//     read one id down, Steroids, Magic Belt, Protect, Speed, Might):
//     0x4C01F0..0x4C1461, 17 functions. A kind-2 task whose kind (0..3, by the
//     ability id) picks the colours; a ring child and four spike children
//     (kind 1, parameter 5), a CLUT strip copy, a tint fade, and a last child
//     (kind 1, parameter 0x48) whose type depends on 0x4FB6F0.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase, and the .data
// tables read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful replacement,
// except that an index past a dispatch table aborts where the original would
// call through the bytes after it (docs/magic_fx_reached.md section 3, the
// precedent), and BuffSpike_Draw aborts where the original would store a
// depth outside its four-entry stack array.
#include "game/magic_s18.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// Buff_Start's CLUT strip: 16 words at +0 and 16 at +0x20, copied over.
// kClutFrom: now Gfx_ClutStripSource[0x1a00] (symbols.toml).
// kClutTo: now Gfx_ClutStrip[0x1a00] (symbols.toml).
// A byte per kind, Buff_Fade's first argument to 0x4FB6F0 (in the effect
// library's .data, 4 bytes below FxDim_Phases; not this group's to name).
// kKindByte: now MagicFx_BuffStats (symbols.toml).

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

unsigned char* SC() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Slot(unsigned char k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride); }
std::int32_t Ptr(const unsigned char* p) { return static_cast<std::int32_t>(Low32(p)); }
void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }

// The scratch words SharedScratch + k (the PSX scratchpad's) and the vertex
// scratch Prim_VertexScratch + k: three SVECTORs at +0, +8, +0x10, +0x18.
constexpr ::bof3::image::Va kDs{SharedScratch_addr};
// kVx: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch
short Ds(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(kDs.v) + k))); }
void SetDs(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kDs.v) + k), v); }
short Vx(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k))); }
void SetVx(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v); }
const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

float* Fl(unsigned char* p) { return reinterpret_cast<float*>(p); }

// A .data dispatch table read in place: the cell called, the index checked
// (the originals' is not).
void CallCell(const unsigned long* table, unsigned entries, unsigned index, const char* who) {
    if (index >= entries) bof3::Fatal("%s: index %u, past the %u-entry table", who, index, entries);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// A stack table's handler by the phase +1.
void CallPhase(const std::uint32_t* phases, unsigned entries, const char* who) {
    const unsigned phase = SC()[1];
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
    magic_harness::Phase(phases[phase])();
}

void LinkAt(unsigned dy, unsigned size) {
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                static_cast<int>(dy), size);
}

long Rtp3(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp3Fn>(&::Gte_RotTransPers3))(VxP(0), VxP(8), VxP(0x10), Fl(p + 8), Fl(p + 0x18), Fl(p + 0x28), &depth,
                                                     &flag);
}
long Rtp4(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp4Fn>(&::Gte_RotTransPers4))(VxP(0), VxP(8), VxP(0x10), VxP(0x18), Fl(p + 8), Fl(p + 0x18),
                                                     Fl(p + 0x28), Fl(p + 0x38), &depth, &flag);
}
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

}  // namespace

#define MS18_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC079 (row 52): the task and its four children

// ===========================================================================
// MAGIC082 (row 32): the task, the ring and the four spikes

void MagicS18_Inject() {
    if (bof3::WantsShadow("magic_s18")) magic_s18::SelfTest();
}

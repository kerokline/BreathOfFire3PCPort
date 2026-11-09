// Three spell overlays compiled into the exe, round nine group S35
// (docs/magic_s35.md): the PSX's MAGIC167, MAGIC168 and MAGIC169.EMI,
// Magic_Rows rows 91, 117 and 97. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Last Resort, Cure and Benediction; the
// names below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC167 0x4EF620..0x4F0666: one ring child and eight beam children
//     (kind 1, 0x47) at the source sprite; the ring draws a gouraud disc and a
//     band round it, each beam a column of four gouraud quads a row, rising
//     round the centre, and two trails of flat tiles;
//   - MAGIC168 0x4F0670..0x4F119E: a pool of 128 motes of its own
//     (0x6AE910, 0x2C bytes each, 0x6AFF10 the current one), each flying up
//     from the owner and drawn as an eight-point star of gouraud triangles and
//     two sets of gouraud lines;
//   - MAGIC169 0x4F11A0..0x4F1E36: a textured halo over the side's centre and,
//     when the target is the party side, one child (kind 1, 8) per party
//     member that tints the member and releases 24 motes of a second pool
//     (0x6AFF18, 0x84 bytes each, run as tasks), which spiral up round it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s35.h"

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

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// MAGIC168's .data, indexed by a mote's kind (Cure_Start sets 3) or by its
// number: the motes per kind, their delay step and lifetime base, a colour
// triple per kind and variant, a start offset per number.
// kCureOffsets: now CureMote_Offsets (symbols.toml). 32 used (& 0x1F) of s16 dx, dy
// kCureColours: now CureMote_Colours (symbols.toml). 3 bytes by kind x 4 + variant
// kCureCounts: now Cure_MoteCounts (symbols.toml).
// kCureDelays: now Cure_MoteDelays (symbols.toml).
// kCureLifetimes: now CureMote_Lifetimes (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::int32_t Mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t Add32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `cdq / and edx, 2^n - 1 / add / sar n` (or `cdq / sub eax, edx / sar 1`):
// a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `fild dword` then `fst(p) dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using FnAlloc = std::uint32_t (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }
unsigned Alloc(std::uint32_t address) { return MH_WORD(FnAlloc, address)() & 0xFFu; }

// The one tail jump to another unit (docs/magic_s35.md section 3), S37's
// MagicFx_FreeCurrentRecord, is ours now and called by name.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

}  // namespace

#define S35_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC167 (row 91, Last Resort read one id down)

// ===========================================================================
// MAGIC168 (row 117, Cure read one id down)

// ===========================================================================
// MAGIC169 (row 97, Benediction read one id down)

void MagicS35_Inject() {
    if (bof3::WantsShadow("magic_s35")) magic_s35::SelfTest();
}

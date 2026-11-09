// Three spell overlays compiled into the exe, round nine group S26
// (docs/magic_s26.md): the PSX's MAGIC114, MAGIC115 and MAGIC117.EMI,
// Magic_Rows rows 11, 115, 73 and 106. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Fire Whip, Remedy, Rest / Snooze and
// Douse; the names below carry the overlay's number instead, and say what the
// code does.
//
//   - MAGIC114 0x4D7960..0x4D9437: a ring of eight triangles round the caster,
//     a volley of kind-1 children (task 7) that each draw a trail, then a beam
//     of three strips and a sprite at the far end; rays of lines at the close;
//   - MAGIC115 0x4D9440..0x4D9ADA: the source sprite tinted and CLUT row 26
//     marked, a child (task 0x30) circling the owner and dropping motes;
//   - MAGIC117 0x4D9F40..0x4DA214: two tint pulses on the source sprite (row
//     73: brighter twice; row 106: darker once), then a flash on the target.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s26.h"

#include <cstdint>

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
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::SetLong;

// SharedScratch's first dword (0x903850; a word at +0 and +2 in the beams)
// and Prim_VertexScratch's four SVECTORs (0x9037A0..0x9037BF): read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).
// The sprite frame-offset table pointer (sprite_pose.h): the effects' bank
// 0x8E3580 around a sprite child's phase, the engine's 0x8B3580 after.
// kFrameSet: now BattleE2_AnimSet (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
unsigned char* TintRecord(unsigned i) { return MoveScript_TintRecords + 12 * i; }
std::uint32_t Key(const void* p) { return Low32(p); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::int32_t AddL(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t MulL(std::int32_t a, unsigned b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * b); }
std::int32_t Shl(std::int32_t a, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }
void SetL(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S26_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC114 (row 11, Fire Whip read one id down)

// ===========================================================================
// MAGIC115 (row 115, Remedy read one id down)

// ===========================================================================
// MAGIC117 (rows 73 and 106: Rest / Snooze and Douse read one id down)

void MagicS26_Inject() {
    if (bof3::WantsShadow("magic_s26")) magic_s26::SelfTest();
}

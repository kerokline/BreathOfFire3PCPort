// Spell group S19 of round nine (docs/magic_s19.md): two BMAGIC overlays
// compiled into the exe.
//
//   MAGIC083 (Magic_Rows row 94, 0x4C1470..0x4C27E6; ability id 0x53, Shield
//   read one id down): a column of light on every member of the target side
//   and three crystals circling each - the aura tasks (kind 1, parameter
//   0x22) and the crystals, which run from a pool of their own at 0x68FA78
//   that the effect's task steps itself.
//
//   MAGIC086 (row 28, 0x4C27F0..0x4C3487; ability id 0x56, no English label
//   one id down): a disc and a rising ring on the source sprite (kind 1,
//   parameter 0x1D), its tint brightened and faded.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase, and .data
// cells read in place), so the start-up fuzz stands recorders in for ours as
// for the originals' copies. No divergence: each is a faithful replacement,
// except that a phase past one of the two stack tables, a face index past the
// crystal's four-entry depth array, or a ring's link made before its point
// was ever computed aborts where the original would read or write its own
// stack (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s19.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

#define MS19_EXPORT extern "C" __attribute__((disable_tail_calls))

// Declared first: ours call each other through MH_CALL.
MS19_EXPORT unsigned char __cdecl Shield_Kind(void);
MS19_EXPORT void __cdecl ShieldAura_DrawDisc(void);
MS19_EXPORT void __cdecl ShieldAura_DrawHalo(void);
MS19_EXPORT void __cdecl ShieldSpark_Dispatch(void);
MS19_EXPORT void __cdecl ShieldSpark_DrawCrystal(void);
MS19_EXPORT unsigned char __cdecl ShieldSpark_Alloc(void);
MS19_EXPORT void __cdecl BarrierDisc_Draw(void);
MS19_EXPORT void __cdecl BarrierRing_Draw(void);
MS19_EXPORT void __cdecl BarrierLine_Draw(void);

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// MoveScript_TintRecords: 12 bytes each, +2..+4 the three levels.
// kTints: now MoveScript_TintRecords (symbols.toml).

// The GTE's input vectors (four SVECTORs) and the effects' scratch words.
// kV0: now Prim_VertexScratch (symbols.toml).
// kV1: now Prim_VertexScratch[1] (symbols.toml).
// kV2: now Prim_VertexScratch[2] (symbols.toml).
// kV3: now Prim_VertexScratch[3] (symbols.toml).
constexpr ::bof3::image::Va kRadius{SharedScratch_addr}; // s16 (dword for MAGIC086's draws)
constexpr ::bof3::image::Va kAngle{0x903854};    // s16 (dword: MAGIC086's inner radius)

// .data (MAGIC083's; the colour tables by kind, 0..3, and face).
// kTopColours: now ShieldSpark_TopColours (symbols.toml). ShieldSpark_TopColours: 4 kinds x 4 faces x rgb
// kBottomColours: now ShieldSpark_BottomColours (symbols.toml). ShieldSpark_BottomColours
// kDiscColours: now ShieldAura_DiscColours (symbols.toml). ShieldAura_DiscColours: 4 kinds x rgb
// kHaloColours: now ShieldAura_HaloColours (symbols.toml). ShieldAura_HaloColours
// The handler tables, read in place by the dispatchers (index unchecked).
// kAuraTypes: now ShieldAura_Types (symbols.toml). ShieldAura_Types, by +1
// kAuraPhases: now ShieldAura_Phases (symbols.toml). ShieldAura_Phases, by +2
// kSparkTypes: now ShieldSpark_Types (symbols.toml). ShieldSpark_Types, by +1
// kSparkPhases: now ShieldSpark_Phases (symbols.toml). ShieldSpark_Phases, by +2
// kPartTypes: now BarrierPart_Types (symbols.toml). BarrierPart_Types, by +1
// kDiscPhases: now BarrierDisc_Phases (symbols.toml). BarrierDisc_Phases, by +2
// kRingPhases: now BarrierRing_Phases (symbols.toml). BarrierRing_Phases, by +2
// kLinePhases: now BarrierLine_Phases (symbols.toml). BarrierLine_Phases, by +2

// Callees in other groups' units, ours now, by name: S18's
// MagicFx_DrawDiscRadius, S37's MagicFx_FreeCurrentRecord, the effect
// library's MagicFx_BuffPopup and MagicFx_LinkByDepth (through LinkSortedFn:
// the depths as long *).

using Handler = void (__cdecl*)();

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }
unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + 12 * i); }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// A .data handler table's cell, read in place.
Handler Cell(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

void Put16(std::uint32_t a, std::uint32_t v) { SetWord(Mem(a), v & 0xFFFF); }
// imul then sar, as the original's 32-bit registers wrap.
std::int32_t MulSar(std::int32_t a, std::int32_t b, int n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
std::int32_t ShlSar(std::int32_t a, int l, int r) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << l) >> r; }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }
float* At(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }
unsigned char* Prim() { return Gfx_PacketNext; }

void DrawModeAndCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// The sin / cos helpers at radius r: (f(angle) * r) >> 12, as a word.
std::int32_t Sin(std::int32_t angle) { return MH_CALL(Math_Sin)(angle); }
std::int32_t Cos(std::int32_t angle) { return MH_CALL(Math_Cos)(angle); }

}  // namespace

// ============================================================================
// MAGIC083 (row 94)
// ============================================================================

// ============================================================================
// MAGIC086 (row 28)
// ============================================================================

void MagicS19_Inject() {
    if (bof3::WantsShadow("magic_s19")) magic_s19::SelfTest();
}

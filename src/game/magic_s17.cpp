// Round nine group S17: three BMAGIC overlays compiled into the exe, 48
// functions at 0x4BCBF0..0x4BEB45 (docs/magic_s17.md):
//
//   - MAGIC075, Magic_Rows row 46 (ids 0x4B / 0xB3, Purify read one id
//     down): a glow on the source sprite that brightens as seven motes land
//     round the target, then fades - 0x4BCBF0..0x4BCFA6;
//   - MAGIC077, row 30 (ids 0x4C / 0xB4 and 0x4D / 0xB5, Raise Dead and
//     Resurrect): a textured halo over the target that sheds motes - more,
//     and a larger halo, for any id but 0x4C / 0xB4 - from a pool of 64 at
//     0x68C0B8, run by the effect's own task - 0x4BCFB0..0x4BDAB6;
//   - MAGIC078, row 15 (ids 0x4E / 0xB6, Leech Power): a shell of quads over
//     the caster and a set of rings that rise from the target and merge into
//     it - 0x4BDAC0..0x4BEB45.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. Calls into functions other groups own go by
// name - the effect library's MagicFx_LinkByDepth / _StepToward /
// _NearSprite3D, MAGIC169's BenedictionMote_DrawGlow, MAGIC219's
// MagicFx_FreeCurrentRecord, and the stack-table phase MAGIC169's
// BenedictionMote_Spiral; MAGIC060's 0x4B1740 is a .data table's entry.
//
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent). The .data tables are
// read in place, their indexes unchecked, as the originals read them.
#include "game/magic_s17.h"

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

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- the addresses ours reads and writes -----------------------------------

constexpr ::bof3::image::Va kScratchA{SharedScratch_addr}; // SharedScratch (the PSX scratchpad): radius, angle words
constexpr ::bof3::image::Va kScratchB{0x903854};
// kVertex: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch: four SVECTORs, 8 bytes apart
constexpr ::bof3::image::Va kFrameSet{0x9039D8};       // the sprite frame-offset table pointer (sprite_pose.h)
// kTints: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords: 12 bytes each, +2..+4 r g b
// kClutRow26: now Gfx_ClutStrip[0x1a00] (symbols.toml). Gfx_ClutStrip row 26 (VRAM row 506)
// kClutRow26Source: now Gfx_ClutStripSource[0x1a00] (symbols.toml).
// kClutRow2: now Gfx_ClutStrip[0x200] (symbols.toml). Gfx_ClutStrip row 2 (VRAM row 482)
// kClutRow2Source: now Gfx_ClutStripSource[0x200] (symbols.toml).

// The .data tables, read in place.
// kPurifyMoteTypes: now PurifyMote_Types (symbols.toml). 1 entry by +1
// kPurifyMotePhases: now PurifyMote_Phases (symbols.toml). 2 entries by +2
// kHaloTypes: now ReviveHalo_Types (symbols.toml). 1 entry by +1
// kHaloPhases: now ReviveHalo_Phases (symbols.toml). 4 entries by +2
// kHaloMoteCounts: now ReviveHalo_MoteCounts (symbols.toml). u8 by the halo's +4
// kMoteTypes: now ReviveMote_Types (symbols.toml). 1 entry by +1
// kLeechOrbTypes: now LeechOrb_Types (symbols.toml). 2 entries by +1
// kShellPhases: now LeechShell_Phases (symbols.toml). 7 entries by +2
// kShellAngles: now LeechShell_Angles (symbols.toml). eleven dwords: the shell's longitudes, then latitudes
// kOrbPhases: now LeechOrb_Phases (symbols.toml). 6 entries by +2

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }
unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + (i & 0xFF) * 12u); }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
std::uint32_t Key(const void* p) { return Low32(p); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int M(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
int Wrap(std::uint32_t v) { return static_cast<int>(v); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0) and its commit.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void LinkAtActor(int dy, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), dy,
                                size);
}

// The target's done flag, 0x904AA8 bit 2, and the task freed: every end of
// an effect here.
void TargetDone() {
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

}  // namespace

#define MS17_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC075 (row 46): the glow and its seven motes

// ===========================================================================
// MAGIC077 (row 30): the halo and its motes

// ===========================================================================
// MAGIC078 (row 15): the shell and the orb

#undef MS17_EXPORT

void MagicS17_Inject() {
    if (bof3::WantsShadow("magic_s17")) magic_s17::SelfTest();
}

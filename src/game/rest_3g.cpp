// Round fourteen group R3G (docs/rest_3g.md): the 32 functions of
// analysis/round14_cut.tsv's group R3G, 0x4925C0..0x5171FB, each read with
// capstone to its last instruction (2026-10-04). A mixed band - what round
// thirteen's effect groups left and the catalog's top-level rows between them:
//
//   kinds 0xAC, 0xAD, 0xAE, 0xBA   effect states the effect groups' tables
//                   point at (E4F's EffectKindAC/AD/AE/BA_States)
//   Screen_TriangleWinding   the z of a screen triangle's cross product
//   Battle_PlaceBossActors / _PlaceBossActor, BattleEnemy_ClearStates
//                   the battle's boss actors copied into the enemy records
//   GameMode8..11_*  four game modes' step dispatchers and steps
//   Quake_VertexLift  a BMAGIC cell vertex's height from Quake's lift table
//   Area109_SwitchHook / _SwitchPattern   area 109's switch cell
//   EffectKind18Sub41_DrawPanels / _DrawRings   sub-kind 0x41's two draws
//   AreaMap_FrameAreaBD, AreaMapBD_BuildView, AreaMapBD_CellTexture
//                   area 0xBD's view, built from a 16 x 16 block map
//   EffectKind0F_CharCount   a string's characters (a byte above 0x7F takes two)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call; a
// memory cell the original reads after a call is read after it here, each
// call's answer into a local first. No divergence: each is a faithful
// replacement. Where the original jumps through a step table past its end or
// indexes past a table, ours aborts with a message (docs/rest_3g.md section 7).
// AreaMapBD_BuildView reads its draw-item array, its item bound and its four
// cull bounds as DIV-0062 (draw_pool.h) and DIV-0041 (widescreen.h) hold the
// operands they patch, as the original reads the operands
// (docs/operand-store.md).
#include "game/rest_3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g_callees.h"
#include "game/cheats.h"
#include "game/map_rim.h"
#include "game/scenario_harness.h"
#include "game/draw_pool.h"
#include "game/psp_code.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }
void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }
U AddressOf(const void* p) { return Low32(p); }
// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U FtolRest3g(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

}  // namespace

// ===========================================================================
// Kind 0xAC: EffectKindAC_States (E4F's), three states - a fade in and out
// ===========================================================================

// ===========================================================================
// Kind 0xAD: EffectKindAD_States (E4F's), states 0 and 1 - a ring rising at
// the third party member
// ===========================================================================

// ===========================================================================
// Kind 0xAE: EffectKindAE_States (E4F's), state 0
// ===========================================================================

// ===========================================================================
// Kind 0xBA: EffectKindBA_States (E4F's), state 1 - the line from extra
// sprite 1's ring
// ===========================================================================

// ===========================================================================
// A screen triangle's winding
// ===========================================================================

// ===========================================================================
// The boss actors at a boss encounter
// ===========================================================================

// ===========================================================================
// Game modes 8..11 (GameMode_Handlers 8..11) and their steps
// ===========================================================================

// ===========================================================================
// Quake's vertex lift (MapCell_DrawTexQuads, _DrawShadedQuads, _DrawSpinQuads)
// ===========================================================================

// ===========================================================================
// Area 109's switch (Area_CellHooks' entry for area 0x6D)
// ===========================================================================

// ===========================================================================
// Kind 0x18 sub-kind 0x41's two draws (EffectKind18Sub41_Hold's)
// ===========================================================================

// ===========================================================================
// Area 0xBD's view
// ===========================================================================

namespace {
// The two y bounds' operands, which nothing patches: the .rdata floats 120.0
// and 121.0 they name - the function's own, ours since the function is.
constexpr U kWideYFloat = 0x5C4244, kNarrowYFloat = 0x5C4238;
}  // namespace

// ===========================================================================
// Kind 0xF's character count
// ===========================================================================

void Rest3G_Inject() {
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kWideYAt.v, &kWideYFloat, 4);
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kNarrowYAt.v, &kNarrowYFloat, 4);
    if (bof3::WantsShadow("rest_3g")) rest_3g::SelfTest();
    // DIV-0187 writes a boss's +0x8F here; Capcom's Battle_PlaceBossActor
    // would clear it and the cheat would miss every boss.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_PlaceBossActor))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_PlaceBossActor ours, and it is disabled");
}

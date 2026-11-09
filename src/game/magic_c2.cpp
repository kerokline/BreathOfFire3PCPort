// Four enemy-skill overlays compiled into the exe, round nine group C2
// (docs/magic_c2.md): the PSX's MAGIC057, MAGIC081, MAGIC116 and
// MAGIC129.EMI, Magic_Rows rows 84, 85, 59 and 145. Read one id down
// (docs/cut-content.md section 2) they are TCRF's Bone Dance, RottenBreath,
// UtmostAttack and Holocaust, which TCRF lists among the enemy-only skills;
// the names below use those labels as hypotheses and say what the code does.
//
//   - MAGIC057 0x4AE7C0..0x4AF035: the caster's animation 3, a camera shake of
//     three beats, then 32 falling bones (kind-1 tasks 0x41) that land on the
//     map's elevation, and a follower kind no creator here starts;
//   - MAGIC081 0x4BF8D0..0x4C01E6: a cloud task that emits up to 48 motes
//     (the pool 0x68E1B8) which swirl and fly at a task kept on the target
//     side's centre, each a textured quad;
//   - MAGIC116 0x4D9AE0..0x4D9F35: 0x78 frames of streaks, eight a frame from a
//     pool of 128 (0x698EC0), each a gouraud quad between two moving points
//     projected through Capcom's unnamed 0x494110;
//   - MAGIC129 0x4E3260..0x4E4416: a beam from the acting actor's enemy record
//     to each party member present, drawn as a band of gouraud quads, and
//     sparks (the pool 0x6A2D38) rising round it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent). The .data dispatches are
// read in place and unchecked, as the originals read them.
#include "game/magic_c2.h"

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
using magic_harness::Pointer;

// kV: now Prim_VertexScratch (symbols.toml).
// The sprite frame-offset table pointer (magic_s17.cpp's kFrameSet): the
// bones switch it to 0x8E3580 around their phase and put back 0x8B3580.
constexpr ::bof3::image::Va kFrameSet{0x9039D8};
// Other units' functions (docs/magic_c2.md section 6) are ours now and called
// by name: the engine's MagicFx_WaitOwnerAnim, S38's MagicFx_CountDownFlag10,
// S37's MagicFx_FreeCurrentRecord.

unsigned char* Sc() { return Sprite_Current; }
// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner))));
}

// The caster's animation, run with Sprite_Current its sprite and put back.
void CasterAnimation(unsigned animation) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current = self;
}

}  // namespace

// ===========================================================================
// MAGIC057 (row 84, Bone Dance read one id down)

// ===========================================================================
// MAGIC081 (row 85, RottenBreath read one id down)

// ===========================================================================
// MAGIC116 (row 59, UtmostAttack read one id down)

// ===========================================================================
// MAGIC129 (row 145, Holocaust read one id down)

void MagicC2_Inject() {
    if (bof3::WantsShadow("magic_c2")) magic_c2::SelfTest();
}

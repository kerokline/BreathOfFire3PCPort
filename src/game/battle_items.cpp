// The battle's item and ability effects, the battle actor's small helpers and
// four PSX primitive setters. docs/battle_items.md.
//
//   - The Healing Herb's sparkles (MAGIC070.EMI on the PSX, compiled into the
//     exe at 0x4B8BD0..0x4B992F): the type dispatch 0x4B8FE0, the three
//     phases 0x4B9090 / 0x4B9200 / 0x4B9270 that 0x4B9000 (left Capcom's)
//     picks, the three draws 0x4B9300 / 0x4B9490 / 0x4B9680, the pool's
//     alloc and free 0x4B98B0 / 0x4B9900.
//   - Three effect draws shared by several MAGIC overlays (the PSX copies
//     read are Sacrifice's, MAGIC 6bdc2d45): 0x4AD6F0, 0x4C5150, 0x4C54F0, and
//     the actor-matrix push 0x4B7D40 they are drawn under.
//   - The battle actor's helpers 0x4FB830..0x4FC1F0 (BATTLE.EMI section 15).
//   - libgpu's SetPolyG3, SetLineG2, SetLineG3 and SetTile1, the port's wider
//     primitives, next to psx_gpu.cpp's.
//   - The SND stream's play and stop 0x5A7140 / 0x5A71C0.
//
// Every call goes through battle_items::g (battle_items_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. No divergence: each is a faithful replacement.
#include "game/battle_items.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_items_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_items {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kCommitSorted.v)),
    Math_Sin,
    Math_Cos,
    Gpu_SetPolyG3,
    Gpu_SetLineG2,
    Gpu_SetLineG3,
    Gpu_SetTile1,
    Gpu_SetPolyGT4,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    As<long (__cdecl*)(const short*, float*, long*, long*)>(&Gte_RotTransPers),
    As<long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*)>(&Gte_RotTransPers3),
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                       long*)>(&Gte_RotTransPers4),
    As<void (__cdecl*)(unsigned char*)>(&Gte_PrimDepths3_10B),
    As<void (__cdecl*)(unsigned char*)>(&Gte_PrimDepths4_14),
    Gte_StoreDepthF,
    Gte_PushMatrix,
    As<void (__cdecl*)(const short*, long*, long*)>(&Gte_RotTrans),
    Gte_RotMatrix,
    Gte_MulMatrix0,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Rand,
    As<void (__cdecl*)(unsigned)>(&Sprite_SetAnimation),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kEnemyAnimation.v)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kActorIsOut.v)),
    Fn<unsigned char (__cdecl*)(unsigned char*, unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kSetTint.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kActorSound.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kEnemySound.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kPlayById.v)),
    SndStream_Stop,
    As<void* (__cdecl*)(const unsigned char*)>(&SndBuf_CreateFromWave),
    Wave_FindData,
    SndBuf_Write,
    Sparkle_Free,
};
Callees g = kOriginals;

}  // namespace battle_items

using namespace battle_items;

namespace {

unsigned char* Actor() { return battle_items::At(BattleE2_Owner); }

// The scratch words: SharedScratch + k, and the vertex scratch + k.
unsigned char* Scr(unsigned k) { return battle_items::At(::bof3::Img(battle_items::at::kScratch.v) + k); }
short S16BattleItems(const unsigned char* at) { return static_cast<short>(battle_items::Word(at)); }
unsigned char* Vtx(unsigned k) { return battle_items::At(::bof3::Img(Prim_VertexScratch_addr) + k); }
const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int Mul12(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12;
}

// `fild dword` then `fstp dword` of an int: the original's integer vertex as
// a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}


}  // namespace

// ===========================================================================
// The PSX primitive setters (libgpu's, on the port's wider primitives)

// ===========================================================================
// The Healing Herb's sparkles (MAGIC070.EMI)

// ===========================================================================
// The shared effect draws

// ===========================================================================
// The battle actor's helpers (BATTLE.EMI section 15 on the PSX)

// ===========================================================================
// The SND stream

// ===========================================================================

void BattleItems_Inject() {
    if (bof3::WantsShadow("battle_items")) battle_items::SelfTest();
}

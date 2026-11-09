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

// What the setters leave at +0x10 and every 0x10 after it: the float 0.01, as
// psx_gpu.cpp's do (docs/psx-library-layer.md section 1).
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;

void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

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

// A method of the IDirectSoundBuffer the SND stream holds, by the 32-bit
// vtable offset: entry offset / 4, as sound_callees.h's Method<> reads it,
// so the index is right where an entry is 8 bytes (an object of ours on
// x86-64; docs/x64-build-design.md 6.1).
template <typename F> F MethodBattleItems(void* object, unsigned offset) {
    void* const* vtable;
    std::memcpy(&vtable, object, sizeof vtable);
    return reinterpret_cast<F>(vtable[offset / 4]);
}
::bof3::Slot32<void>& StreamBuffer() { return SndStream_Buffer; }   // the 4-byte slot, read and written as one
constexpr long kBufferLost = static_cast<long>(0x88780096u);   // DSERR_BUFFERLOST

}  // namespace

// ===========================================================================
// The PSX primitive setters (libgpu's, on the port's wider primitives)

// original 0x5A75F0 (PSX SetPolyG3 0x8017B2F4): code 0x30, 0.01 per vertex.
extern "C" void __cdecl Gpu_SetPolyG3(unsigned char* prim) {
    prim[7] = 0x30;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOne);
}
// original 0x5A76B0 (PSX SetLineG2 0x8017B40C): code 0x50, two vertices.
extern "C" void __cdecl Gpu_SetLineG2(unsigned char* prim) {
    prim[7] = 0x50;
    PutDword(prim + 0x10, kPointZeroOne);
    PutDword(prim + 0x20, kPointZeroOne);
}
// original 0x5A76D0 (PSX SetLineG3 0x8017B440): code 0x58, three vertices.
// The PSX's also stores the polyline's terminator 0x55555555; the port's
// primitive has none.
extern "C" void __cdecl Gpu_SetLineG3(unsigned char* prim) {
    prim[7] = 0x58;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOne);
}
// original 0x5A7750 (PSX SetTile1 0x8017B3A8): code 0x68, one vertex.
extern "C" void __cdecl Gpu_SetTile1(unsigned char* prim) {
    prim[7] = 0x68;
    PutDword(prim + 0x10, kPointZeroOne);
}

// ===========================================================================
// The Healing Herb's sparkles (MAGIC070.EMI)

// ===========================================================================
// The shared effect draws

// ===========================================================================
// The battle actor's helpers (BATTLE.EMI section 15 on the PSX)

// ===========================================================================
// The SND stream

// original 0x5A7140: nothing without Snd_Device. Else the playing stream
// stopped, a buffer made of the WAV image and kept at 0x7DE3C8,
// SetCurrentPosition(0) and Play; a lost buffer (DSERR_BUFFERLOST) gets the
// wave's data written again (SndBuf_Write restores it) and a second Play.
extern "C" void __cdecl SndStream_Play(const unsigned char* wave) {
    if (sound::MusicLogOn())  // the voice stream's line in the music log (BOF3X_MUSIC_LOG=1; sound.cpp)
        bof3::Log("music_log   f%u SndStream_Play(%p) from %p", Frame_Counter, static_cast<const void*>(wave),
                  __builtin_return_address(0));
    if (Snd_Device == nullptr) return;
    battle_items::g.stream_stop();
    void* const buffer = battle_items::g.create_from_wave(wave);
    StreamBuffer() = buffer;
    if (buffer == nullptr) return;
    MethodBattleItems<long (__stdcall*)(void*, unsigned long)>(buffer, 0x34)(buffer, 0);
    void* b = StreamBuffer();
    if (MethodBattleItems<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0) != kBufferLost)
        return;
    const unsigned char* data = nullptr;
    const unsigned size = battle_items::g.find_data(wave, &data);
    battle_items::g.buffer_write(StreamBuffer(), data, 0, size);
    b = StreamBuffer();
    MethodBattleItems<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0);
}

// original 0x5A71C0: the stream's buffer, if any, stopped when GetStatus says
// it plays (bit 0), released, and forgotten. The original's status word is
// an uninitialised local that GetStatus fills; ours starts at 0.
extern "C" void __cdecl SndStream_Stop(void) {
    void* b = StreamBuffer();
    if (b == nullptr) return;
    unsigned long status = 0;
    MethodBattleItems<long (__stdcall*)(void*, unsigned long*)>(b, 0x24)(b, &status);
    if (status & 1) {
        b = StreamBuffer();
        MethodBattleItems<long (__stdcall*)(void*)>(b, 0x48)(b);
    }
    b = StreamBuffer();
    MethodBattleItems<unsigned long (__stdcall*)(void*)>(b, 8)(b);
    StreamBuffer() = nullptr;
}

// ===========================================================================

void BattleItems_Inject() {
    if (bof3::WantsShadow("battle_items")) battle_items::SelfTest();
    BOF3_INJECT(Gpu_SetPolyG3);
    BOF3_INJECT(Gpu_SetLineG2);
    BOF3_INJECT(Gpu_SetLineG3);
    BOF3_INJECT(Gpu_SetTile1);
    BOF3_INJECT(SndStream_Play);
    BOF3_INJECT(SndStream_Stop);
}

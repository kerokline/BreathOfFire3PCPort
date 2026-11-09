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

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// field_frame.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// `mov al, [eax + 8]; add al, dl; push eax`: the actor's animation byte plus
// the offset, in the low byte of the actor's own pointer. In inline assembly
// because clang 22.1.8 folds the C++ spelling of a pointer's high bytes
// merged with a byte read through it (docs/event-ops.md section 8).
std::uint32_t AnimationInPointer(const unsigned char* actor, unsigned offset) {
    std::uint32_t v = Low32(actor);
    __asm__ volatile("movb 8(%1), %b0\n\taddb %b2, %b0" : "+q"(v) : "r"(actor), "q"(offset) : "memory", "cc");
    return v;
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

// original 0x4FB830 (PSX 0x800ABEC0): with Sprite_Current the effect's actor
// (0x93B940) for the call and put back after, a party member (0x904B34 below
// 3) gets Sprite_SetAnimation(its +8 + offset), an enemy 0x435A20(the index,
// arg). As the original has it: the animation byte goes in the low byte of
// the actor's own pointer, pushed whole; the enemy's index in the low byte of
// the caller's ecx (0x435A20 reads only that byte).
extern "C" void __cdecl BattleActor_SetAnimation(unsigned offset, unsigned arg) {
    const unsigned char index = battle_items::At(::bof3::Img(at::kActorIndex.v))[0];
    unsigned char* const actor = Actor();
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = actor;
    if (index < 3) battle_items::g.set_animation(AnimationInPointer(actor, offset));
    else battle_items::g.enemy_animation(index, arg);
    Sprite_Current = saved;
}

// original 0x4FBD10 (PSX 0x800AC68C): Sprite_Current's screen point. The
// actor's (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)) projected into a
// TILE_1 at Gfx_PacketNext - never committed, used for its vertex - and the
// float x / y truncated (the CRT's _ftol) to +0x2E / +0x30. The PSX reads the
// integer screen point back the same way.
extern "C" void __cdecl BattleActor_UpdateScreenXY(void) {
    const unsigned char* const sc = Sprite_Current;
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((battle_items::Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((battle_items::Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16BattleItems(sc + 0x3E) / 2));
    v[3] = 0;
    battle_items::g.set_tile1(p);
    long depth, flag;
    battle_items::g.rtp(v, reinterpret_cast<float*>(p + 8), &depth, &flag);
    battle_items::g.store_depth(reinterpret_cast<float*>(p + 0x10));
    battle_items::SetWord(Sprite_Current + 0x2E, Ftol16(static_cast<std::uint32_t>(battle_items::Long(p + 8))));
    battle_items::SetWord(Sprite_Current + 0x30, Ftol16(static_cast<std::uint32_t>(battle_items::Long(p + 0xC))));
}

// original 0x4FBDB0 (PSX 0x800AC73C): unless actor `index` is out
// (0x4456C0), a party member with +0x90 bit 7 or an enemy with +0x92 bit 7
// is tinted (-6, -10, 0, 0).
extern "C" void __cdecl BattleActor_Flash(unsigned index) {
    if (battle_items::g.is_out(index) != 0) return;
    const unsigned i = index & 0xFF;
    if (i < 3) {
        unsigned char* const r = battle_items::At(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride);
        if (r[0x90] & 0x80) battle_items::g.set_tint(r, 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
        return;
    }
    unsigned char* const r = battle_items::At(::bof3::Img(Battle_Enemies_addr) + (i - 3) * at::kEnemyStride);
    if (r[0x92] & 0x80) battle_items::g.set_tint(r, 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
}

// original 0x4FC030 (PSX 0x800ACBC0): the acting actor's sound. A party
// member becomes Field_State, then its sound `first` + 1 (unless first is 0)
// and `second` (unless 0) through 0x446A50; an enemy plays its record's cue
// word (+0xF8's) through 0x437450 when 0x904AAA is set, else Sound_PlayById
// 0x600 + 2 * its type byte +0xF0. The arguments' low bytes are what is read.
extern "C" void __cdecl BattleActor_PlaySound(unsigned first, unsigned second) {
    const unsigned char index = battle_items::At(::bof3::Img(at::kActorIndex.v))[0];
    if (index < 3) {
        Field_State = battle_items::At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride);
        const auto a = static_cast<unsigned char>(first);
        if (a != 0) battle_items::g.actor_sound(static_cast<unsigned char>(a + 1));
        if (static_cast<unsigned char>(second) != 0) battle_items::g.actor_sound(second);
        return;
    }
    auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(battle_items::At(::bof3::Img(Battle_Enemies_addr) + (index - 3u) * at::kEnemyStride));
    if (EventBattle_Id != 0) {
        const auto* const cue = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::int32_t>(enemy_rec->cue_table)));
        battle_items::g.enemy_sound(battle_items::Word(cue));
    } else {
        battle_items::g.play_by_id(enemy_rec->data_id * 2u + 0x600);
    }
}

// original 0x4FC1F0: the acting actor's effect size (callers store it to
// Sprite_Current +9, which the effect draws scale by). A party member: the
// byte of 0x65C3F8 by 0x904B89 when its +0x134 has bit 1, else of 0x65C3EC by
// its +0x89; an enemy: the byte at 0x8C5652 + 0x8C * its type. Only al is
// defined in the original's answer; all ten callers store al.
extern "C" unsigned char __cdecl BattleActor_FxSize(void) {
    const unsigned char index = battle_items::At(::bof3::Img(at::kActorIndex.v))[0];
    if (index < 3) {
        const unsigned char* const r = battle_items::At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride);
        if (r[0x134] & 2) return battle_items::At(::bof3::Img(at::kFxSizeAltTable.v))[BattlePhases_B89];
        return battle_items::At(::bof3::Img(at::kFxSizeTable.v))[r[0x89]];
    }
    const unsigned char type = battle_items::At(::bof3::Img(Battle_Enemies_addr) + (index - 3u) * at::kEnemyStride + 0xF0)[0];
    return battle_items::At(::bof3::Img(at::kEnemyFxSize.v) + type * 0x8Cu)[0];
}

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
    BOF3_INJECT(BattleActor_SetAnimation);
    BOF3_INJECT(BattleActor_UpdateScreenXY);
    BOF3_INJECT(BattleActor_Flash);
    BOF3_INJECT(BattleActor_PlaySound);
    BOF3_INJECT(BattleActor_FxSize);
    BOF3_INJECT(SndStream_Play);
    BOF3_INJECT(SndStream_Stop);
}

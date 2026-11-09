// Moved here by tools/move_funcs.py from magic_s20.cpp (docs/source-layout-round.md).
#include "game/magic/magic_088.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s20.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kM088Colours{0x65B52C};       // u8 rgb rows, row = +4 * 6 + band

constexpr ::bof3::image::Va kM088ChildTypes{0x65B574};    // by +1: fan, wave

constexpr ::bof3::image::Va kM088FanPhases{0x65B57C};     // by +2: three, other overlays'

constexpr ::bof3::image::Va kM088WaveBands{0x65B588};     // six dwords, stepped +1 / -1

constexpr ::bof3::image::Va kM088WavePhases{0x65B5A8};    // by +2: four

// That table as the switch it was compiled from - the function's logic, ours
// since the function is (docs/operand-store.md): by the ability id - 0x1E,
// the ids 0x1E, 0x58 and 0xBE take case 0, 0x1F, 0x59 and 0xBF case 1, 0x5A
// and 0xC0 case 2; every other id of the 0xA3 case 3.
unsigned VariantCase(unsigned k) {
    switch (k) {
    case 0x00: case 0x3A: case 0xA0: return 0;
    case 0x01: case 0x3B: case 0xA1: return 1;
    case 0x3C: case 0xA2: return 2;
    default: return 3;
    }
}

// The effect's scratch (words 0x903850..0x90385E, Scratch_Swap among them)
// and the vertex scratch (Prim_VertexScratch: SVECTORs 8 bytes apart).
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};

// The ability id (u16) and the actor's side byte 0x904B35 (4 for the party's
// commands, by MAGIC088's reading).
constexpr ::bof3::image::Va kAbility{0x904B80};

constexpr ::bof3::image::Va kSide{0x904B35};

unsigned char* Scr(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }

unsigned char* Vtx(unsigned k) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + k); }

const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char* Task(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * at::kTaskStride); }

unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(0x7E0700) + i * 12); }   // MoveScript_TintRecords, unchecked

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}

void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

// A .data dispatch table's entry, read as the original reads it.
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar`: a 32-bit product that wraps, shifted arithmetically.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}

// A 32-bit add that wraps.
int Add32(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// Sprite_Current's position from `from` (+0x34 / +0x38 / +0x3C).
void CopyPosition(unsigned char* to, const unsigned char* from) {
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

// MapView_LinkPrimAt at Sprite_Current's position (read now).
void LinkAtCurrent(unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), 2, size);
}

}  // namespace

#define S20_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// Magic088_DrawWave's z: -(sin(0x903856) * 0x903858 >> 12 + the dword
// 0x903858) - the dword, so the high word is 0x90385A's, and only the low
// word is kept.
std::uint16_t WaveZ(int s) {
    return static_cast<std::uint16_t>(0u - static_cast<std::uint32_t>(Add32(MulSar(s, S16(Scr(8)), 12), Long(Scr(8)))));
}

}  // namespace

// original 0x4C4490: the kind-2 task: its phase +1 through a seven-entry
// table on its stack - Magic088_Start, _TintOn, _Darken, 0x4EF7C0 (MAGIC167's:
// on once +0xB is 1 or less), _Lighten, _Apply, 0x4E5200 (MAGIC131's: the
// done flag and free once +0xB is 0) - unchecked; ours aborts past it.
S20_EXPORT void __cdecl Magic088_Task(void) {
    static constexpr std::uint32_t kPhases[7] = {bof3::addr::Magic088_Start,
                                                 bof3::addr::Magic088_TintOn,
                                                 bof3::addr::Magic088_Darken,
                                                 bof3::addr::LastResort_WaitChildren,
                                                 bof3::addr::Magic088_Lighten,
                                                 bof3::addr::Magic088_Apply,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 7) bof3::Fatal("Magic088_Task: phase %u, past the seven-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C44E0: the source sprite's (0x904B4C, read once) position; +4
// the variant (Magic088_Variant), +0xB 0, +9 0x10, on; two kind-1 children
// (parameter 0x20) owned by this task and counted in +0xB, at its position:
// type 0 (+9 and +0xA 0) and type 1 (+4 and +9 this task's, +0xA 0x10).
// Then CLUT strip rows 26 and 27 copied from 0x4000 below; dirty; sound
// 0x100. BattleTask_Create's index is not tested.
S20_EXPORT void __cdecl Magic088_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    CopyPosition(Sprite_Current, src);
    const unsigned char variant = MH_CALL(Magic088_Variant)();
    Sprite_Current[4] = variant;
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x10;
    Bump(Sprite_Current[1], 1);
    unsigned index = MH_CALL(BattleTask_Create)(1, 0x20) & 0xFFu;
    unsigned char* sc = Sprite_Current;
    unsigned char* t = Task(index);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(t)->owner, PtrValue(sc));
    t[1] = 0;
    t[9] = 0;
    t[0xA] = 0;
    CopyPosition(t, sc);
    Bump(sc[0xB], 1);
    index = MH_CALL(BattleTask_Create)(1, 0x20) & 0xFFu;
    sc = Sprite_Current;
    t = Task(index);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(t)->owner, PtrValue(sc));
    t[1] = 1;
    t[4] = sc[4];
    t[9] = sc[9];
    t[0xA] = 0x10;
    CopyPosition(t, sc);
    Bump(sc[0xB], 1);
    for (std::uint32_t k = 0; k < 0x20; k += 2) {
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + k), Word(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + k - 0x4000)));
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3420 + k), Word(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3420 + k - 0x4000)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C4650: phase 1. +9 down; at 0 the source (read once, first)
// marked +0x5C = 1, its tints released and a new one set (0, 0, 0, 1), whose
// index goes to +0xA; +9 8; on.
S20_EXPORT void __cdecl Magic088_TintOn(void) {
    unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    src[0x5C] = 1;
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C46B0: phase 2. The tint record +0xA's (unchecked) bytes +2
// and +3 down; +9 down; at 0 on.
S20_EXPORT void __cdecl Magic088_Darken(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -1);
    Bump(Tint(sc[0xA])[3], -1);
    Bump(sc[9], -1);
    if (Sprite_Current[9] == 0) Bump(Sprite_Current[1], 1);
}

// original 0x4C4700: phase 4. The tint record's +2 and +3 up; when +2 is back
// to 0: the source (read once, first) +0x5C 0, its tints released, the target
// flashed (BattleActor_Flash), on.
S20_EXPORT void __cdecl Magic088_Lighten(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    Bump(Tint(sc[0xA])[2], 1);
    Bump(Tint(sc[0xA])[3], 1);
    if (Tint(sc[0xA])[2] != 0) return;
    src[0x5C] = 0;
    MH_CALL(Sprite_ReleaseTint)(src);
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kTarget))[0]);
    Bump(Sprite_Current[1], 1);
}

// original 0x4C4780: phase 5. Once the children are gone (+0xB 0): the stat
// change 0x4FB6F0(0x65C39C[+4], the target) - the library's; a kind-1 child
// (parameter 0x48) owned by this task with +4 = +4 + 4 when it answers
// non-zero, else 8; +9 1, +0xA 0; the count up; on. The index untested.
S20_EXPORT void __cdecl Magic088_Apply(void) {
    if (Sprite_Current[0xB] != 0) return;
    const unsigned char target = Mem(::bof3::Img(at::kTarget))[0];
    const unsigned char stat = Mem(::bof3::Img(MagicFx_BuffStats_addr))[Sprite_Current[4]];
    const bool took = MH_CALL(MagicFx_ApplyBuff)(stat, target) != 0;
    const unsigned index = MH_CALL(BattleTask_Create)(1, 0x48) & 0xFFu;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(index);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    tt->owner = PtrValue(sc);
    t[4] = took ? static_cast<unsigned char>(sc[4] + 4) : 8;
    t[9] = 1;
    t[0xA] = 0;
    Bump(sc[0xB], 1);
    Bump(Sprite_Current[1], 1);
}

// original 0x4C4830: the variant, by the ability id (word 0x904B80). With the
// side byte 0x904B35 at 4: ids 0x1E..0xC0 index a byte table in .text
// (0x4C488C; ours is VariantCase) choosing one of four answers - 1, 2, 0, 3 - and
// any other id is 3. On the other side: id 0x112 becomes 0x5A and answers 0,
// any other 3. Only al is defined.
S20_EXPORT unsigned char __cdecl Magic088_Variant(void) {
    unsigned char* const id = Mem(::bof3::Img(kAbility.v));
    if (Mem(::bof3::Img(kSide.v))[0] == 4) {
        const unsigned k = Word(id) - 0x1Eu;
        if (k > 0xA2) return 3;
        const unsigned variant = VariantCase(k);
        switch (variant) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 0;
        case 3: return 3;
        default: bof3::Fatal("Magic088_Variant: index %u past the four-entry jump table", (unsigned)variant);
        }
    }
    if (Word(id) != 0x112) return 3;
    SetWord(id, 0x5A);
    return 0;
}

// original 0x4C4930: the child (kind 1, parameter 0x20): a jmp through
// 0x65B574 by +1 (two entries, the fan and the wave, unchecked).
S20_EXPORT void __cdecl Magic088_Child(void) { Entry(::bof3::Img(kM088ChildTypes.v), Sprite_Current[1])(); }

// original 0x4C4950: the fan. Its phase +2 through 0x65B57C (three - other
// overlays' 0x4C2D10, 0x4EF840, 0x4B1740 - unchecked); then while it lives
// the fan under the actor's matrix.
S20_EXPORT void __cdecl Magic088_FanRun(void) {
    Entry(::bof3::Img(kM088FanPhases.v), Sprite_Current[2])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Magic088_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C4980: sixteen gouraud triangles round the matrix's origin,
// radius 0xF0 (0x903850), the centre grey +9 * 12 (0x90385A), the rim dark;
// between two draw-mode packets (tpage 0x55, then 0x15), all at slot 5.
S20_EXPORT void __cdecl Magic088_DrawFan(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(Scr(0), 0xF0);
    SetWord(Scr(0xA), sc[9] * 12u);
    int v = MH_CALL(Math_Sin)(0);
    SetWord(Vtx(0x10), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
    v = MH_CALL(Math_Cos)(0);
    SetWord(Vtx(0x12), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = Word(Vtx(0x12));
        const std::uint16_t x = Word(Vtx(0x10));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        v = MH_CALL(Math_Sin)(a);
        SetWord(Vtx(0x10), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Cos)(a);
        unsigned char* const p = Gfx_PacketNext;
        SetWord(Vtx(0x14), 0);
        SetWord(Vtx(0x12), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        SetWord(Vtx(0xC), 0);
        SetWord(Vtx(4), 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                                   reinterpret_cast<float*>(p + 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = Scr(0xA)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xA)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C4B30: the wave. Its phase +2 through 0x65B5A8 (four - 0x4E47F0,
// another overlay's, then _WaveGrow, _WaveHold, _WaveFade - unchecked); then
// while it lives, past phase 0, the bands stepped and drawn under the
// actor's matrix.
S20_EXPORT void __cdecl Magic088_WaveRun(void) {
    Entry(::bof3::Img(kM088WavePhases.v), Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Magic088_WaveStep)();
    MH_CALL(Magic088_DrawWave)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C4B70: wave phase 1: +9 up; at 0x20 on.
S20_EXPORT void __cdecl Magic088_WaveGrow(void) {
    Bump(Sprite_Current[9], 1);
    if (Sprite_Current[9] == 0x20) Bump(Sprite_Current[2], 1);
}

// original 0x4C4B90: wave phase 2: +0xA down; at 0 the owner's count down and
// on.
S20_EXPORT void __cdecl Magic088_WaveHold(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    Bump(Sprite_Current[2], 1);
}

// original 0x4C4BC0: wave phase 3: +9 down by 2; at 0 freed (a tail jmp).
S20_EXPORT void __cdecl Magic088_WaveFade(void) {
    Bump(Sprite_Current[9], -2);
    if (Sprite_Current[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C4BE0: the six band phases at 0x65B588 (dwords): the even ones
// up by one, the odd ones down.
S20_EXPORT void __cdecl Magic088_WaveStep(void) {
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const d = Mem(::bof3::Img(kM088WaveBands.v) + 4 * i);
        SetLong(d, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(d)) + ((i & 1) ? 0xFFFFFFFFu : 1u)));
    }
}

// original 0x4C4C00: six bands of 64 flat quads round the matrix's origin,
// band b from radius 32 b to 32 (b + 1), each point lifted by the band's
// phase (0x65B588 + 4 b, low nibble) plus its column, as sin((phase + column)
// & 15 << 8) * +9 >> 12 + +9; the band's colour 0x65B52C row +4 * 6 + b
// times +9 / 2. One draw-mode packet first.
S20_EXPORT void __cdecl Magic088_DrawWave(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtCurrent(0xC);
    SetWord(Scr(8), Sprite_Current[9]);
    for (unsigned b = 0; b < 6; ++b) {
        const unsigned char* const phase = Mem(::bof3::Img(kM088WaveBands.v) + 4 * b);
        const unsigned char* const sc = Sprite_Current;
        SetWord(Scr(2), (b + 1) << 5);
        SetWord(Scr(0), b << 5);
        const unsigned row = (b + sc[4] * 6u) * 3u;
        const unsigned half = sc[9] >> 1;
        SetWord(Scr(0xA), Mem(::bof3::Img(kM088Colours.v))[row] * half);
        SetWord(Scr(0xC), Mem(::bof3::Img(kM088Colours.v) + 1)[row] * (sc[9] >> 1));
        SetWord(Scr(0xE), Mem(::bof3::Img(kM088Colours.v) + 2)[row] * (sc[9] >> 1));
        SetWord(Scr(6), (phase[0] & 0xFu) << 8);
        int v = MH_CALL(Math_Cos)(0);
        SetWord(Vtx(8), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        v = MH_CALL(Math_Sin)(0);
        SetWord(Vtx(0xA), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        v = MH_CALL(Math_Sin)(S16(Scr(6)));
        SetWord(Vtx(0xC), WaveZ(v));
        SetWord(Scr(6), ((phase[0] + 1u) & 0xFu) << 8);
        v = MH_CALL(Math_Cos)(0);
        SetWord(Vtx(0x18), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(0);
        SetWord(Vtx(0x1A), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(S16(Scr(6)));
        SetWord(Vtx(0x1C), WaveZ(v));
        for (unsigned c = 1; c < 0x41; ++c) {
            SetWord(Vtx(0), Word(Vtx(8)));
            SetWord(Vtx(2), Word(Vtx(0xA)));
            SetWord(Vtx(4), Word(Vtx(0xC)));
            SetWord(Vtx(0x14), Word(Vtx(0x1C)));
            SetWord(Vtx(0x10), Word(Vtx(0x18)));
            SetWord(Vtx(0x12), Word(Vtx(0x1A)));
            SetWord(Scr(4), c << 6);
            v = MH_CALL(Math_Cos)(S16(Scr(4)));
            SetWord(Vtx(8), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(4)));
            SetWord(Vtx(0xA), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(6)));
            SetWord(Vtx(0xC), WaveZ(v));
            SetWord(Scr(6), ((c + phase[0] + 1u) & 0xFu) << 8);
            v = MH_CALL(Math_Cos)(S16(Scr(4)));
            SetWord(Vtx(0x18), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(4)));
            SetWord(Vtx(0x1A), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(6)));
            unsigned char* const p = Gfx_PacketNext;
            SetWord(Vtx(0x1C), WaveZ(v));
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), reinterpret_cast<float*>(p + 8),
                                       reinterpret_cast<float*>(p + 0x14), reinterpret_cast<float*>(p + 0x20),
                                       reinterpret_cast<float*>(p + 0x2C), &depth);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            p[4] = Scr(0xA)[0];
            p[5] = Scr(0xC)[0];
            p[6] = Scr(0xE)[0];
            LinkAtCurrent(0x38);
        }
    }
}

void Magic088_Inject() {
    BOF3_INJECT(Magic088_Task);
    BOF3_INJECT(Magic088_Start);
    BOF3_INJECT(Magic088_TintOn);
    BOF3_INJECT(Magic088_Darken);
    BOF3_INJECT(Magic088_Lighten);
    BOF3_INJECT(Magic088_Apply);
    BOF3_INJECT(Magic088_Variant);
    BOF3_INJECT(Magic088_Child);
    BOF3_INJECT(Magic088_FanRun);
    BOF3_INJECT(Magic088_DrawFan);
    BOF3_INJECT(Magic088_WaveRun);
    BOF3_INJECT(Magic088_WaveGrow);
    BOF3_INJECT(Magic088_WaveHold);
    BOF3_INJECT(Magic088_WaveFade);
    BOF3_INJECT(Magic088_WaveStep);
    BOF3_INJECT(Magic088_DrawWave);
}

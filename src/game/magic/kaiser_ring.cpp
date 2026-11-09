// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/kaiser_ring.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s30.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
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

// The scratch the overlays keep their working values in: sixteen bytes at
// 0x903850 (words; Scratch_Swap is +0xC) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// kV: now Prim_VertexScratch (symbols.toml).
// The dword the sprite children point at a sprite bank while their phase
// runs (0x8E3580 / 0x813580), and put back to 0x8B3580 after.
// kSpriteBank: now BattleE2_AnimSet (symbols.toml).
// MAGIC131's mote pool: 48 records of 0x2C bytes, and the cell holding the
// mote being run (KaiserMote_Pool, KaiserMote_Current).
// kMotePool: now KaiserMote_Pool (symbols.toml).
constexpr std::uint32_t kMoteStride = 0x2C;

constexpr unsigned kMotes = 48;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Cur() { return Pointer(::bof3::Img(KaiserMote_Current_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddW(unsigned char* a, unsigned v) { SetWord(a, (Word(a) + v) & 0xFFFF); }

void AddL(unsigned char* a, std::int32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + static_cast<std::uint32_t>(v)));
}

// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return Low32(p); }

// `imul` then `sar 0xC` (or `sar 3`): the 32-bit product wraps, the shift is
// arithmetic.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}

int Mul12(int a, int b) { return MulSar(a, b, 12); }

// `shl eax, 6` then `sar eax, 3`.
int Shl6Sar3(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) << 6) >> 3; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// battle_items.cpp's and field_frame.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Mote(unsigned i) { return Mem(::bof3::Img(KaiserMote_Pool_addr) + i * kMoteStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The projections with the arguments the originals push: Gte_RotTransPers4
// gets the depth and flag pointers, Gte_RotTransPers the flag pointer too.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S30_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp4(unsigned char* prim) {
    long depth, flag;
    S30_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &depth, &flag);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A .data dispatch table read in place (the fuzz swaps its entries for
// recorders), the index checked against the table's length.
magic_harness::Handler Entry(const char* who, std::uint32_t table, unsigned entries, unsigned index) {
    if (index >= entries) PastTable(who, index, entries);
    return reinterpret_cast<magic_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A kind-0 or kind-1 mote's position: (sin, cos) of its angle (the scratch
// word +6, re-read for the cosine) times `scale` >> 3 about
// (Field_Kind2X, Field_Kind2Z); `scale` is 0 for the fixed 64 (<< 6).
void MotePlace(unsigned angle, bool by_radius) {
    SetSW(6, angle);
    int v = Sin(static_cast<short>(angle));
    SetLong(Cur() + 0x14, static_cast<std::int32_t>(
                              static_cast<std::uint32_t>(by_radius ? MulSar(v, S16(Cur() + 0xC), 3) : Shl6Sar3(v)) +
                              static_cast<std::uint32_t>(Field_Kind2X)));
    v = Cos(SS(6));
    SetLong(Cur() + 0x18, static_cast<std::int32_t>(
                              static_cast<std::uint32_t>(by_radius ? MulSar(v, S16(Cur() + 0xC), 3) : Shl6Sar3(v)) +
                              static_cast<std::uint32_t>(Field_Kind2Z)));
}

unsigned AngleA() { return (static_cast<unsigned>(Mem(Addr(KaiserMoteA_Angles))[Cur()[7]]) << 7) & 0xFFFF; }

unsigned AngleB() { return (static_cast<unsigned>(Mem(Addr(KaiserMoteB_Angles))[Cur()[7]]) << 6) & 0xFFFF; }

void MoteDraw() {
    Call0(bof3::addr::KaiserMote_UpdateScreenXY);
    Call0(bof3::addr::KaiserMote_Draw);
}

}  // namespace

// original 0x4E5EA0: +2 through KaiserRing_Phases (three entries); while +0
// and +2 are set, the ring under the actor's matrix.
S30_EXPORT void __cdecl KaiserRing_Run(void) {
    Entry("KaiserRing_Run", Addr(KaiserRing_Phases), 3, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::KaiserRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E5EE0: at (Field_Kind2X, Field_Kind2Z), the owner's height;
// radius +0x14 0x40, +0xB 0, +9 0x10, +0xA 1, +2 on.
S30_EXPORT void __cdecl KaiserRing_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, Field_Kind2X);
    rv::Store(rv::Current<records::BattleTask>()->z, Field_Kind2Z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x40);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4E5F40: the radius up by 0x20; at 0x200 +0xB 1 and +2 on.
S30_EXPORT void __cdecl KaiserRing_Grow(void) {
    AddL(Sc() + 0x14, 0x20);
    if (rv::Current<records::BattleTask>()->step_y != 0x200) return;
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4E5F70: the radius up by 0x10, +9 down by 2; at 0 the owner's
// count down and the task freed.
S30_EXPORT void __cdecl KaiserRing_Fade(void) {
    AddL(Sc() + 0x14, 0x10);
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5FB0: a ring of sixteen textured quads (ten drawn: angles
// 0x700..0xE00 skipped) of radius +0x14 from height -0x400 to 0; the blend
// mode +0xB (the draw mode's tpage ((+0xB & 3) << 5) | 0x15); u by the quad's
// parity; shade +9 x 8.
S30_EXPORT void __cdecl KaiserRing_Draw(void) {
    {
        unsigned char* const p0 = Gfx_PacketNext;
        DrawMode(p0, ((Sc()[0xB] & 3u) << 5) | 0x15);
    }
    Commit(3, 0xC);
    {
        const unsigned char* const sc = Sc();
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
        SetSW(8, sc[9] << 3);
        SetSW(0xA, sc[9] << 3);
        SetSW(0xC, sc[9] << 3);
    }
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    std::uint32_t counter = 1;
    for (int angle = 0x100; angle < 0x1100; angle += 0x100, ++counter) {
        const std::uint16_t x = VW(8), z = VW(0xA);
        SetVW(0, x);
        SetVW(2, z);
        SetVW(4, 0xFC00);
        SetVW(0x10, x);
        SetVW(0x12, z);
        SetVW(0x14, 0);
        v = Sin(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(angle);
        unsigned char* const p = Gfx_PacketNext;
        const std::uint16_t nx = VW(8);
        const auto nz = static_cast<std::uint16_t>(Mul12(v, SS(0)));
        SetVW(0xA, nz);
        SetVW(0xC, 0xFC00);
        SetVW(0x18, nx);
        SetVW(0x1A, nz);
        SetVW(0x1C, 0);
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, Sc()[0xB]);
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, Sc()[0xB], 0x340, 0x100);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage & 0xFFFF);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut & 0xFFFF);
        const auto u = static_cast<unsigned char>((counter & 1) << 6);
        p[0x15] = 0;
        p[0x14] = u;
        p[0x25] = 0;
        p[0x24] = static_cast<unsigned char>(u + 0x3F);
        p[0x34] = u;
        p[0x35] = 0xBF;
        p[0x44] = static_cast<unsigned char>(u + 0x3F);
        p[0x45] = 0xBF;
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        if (angle <= 0x600 || angle >= 0xF00) Commit(3, 0x48);
    }
}

// original 0x4E6200: a mote's kind (KaiserMote_Current +1) through
// KaiserMote_Kinds (two entries), a jmp.
S30_EXPORT void __cdecl KaiserMote_Task(void) {
    Entry("KaiserMote_Task", Addr(KaiserMote_Kinds), 2, Cur()[1])();
}

// original 0x4E6220: +2 through KaiserMoteA_Phases (three entries); while +0
// and +2 are set, its screen point and its fan.
S30_EXPORT void __cdecl KaiserMoteA_Run(void) {
    Entry("KaiserMoteA_Run", Addr(KaiserMoteA_Phases), 3, Cur()[2])();
    const unsigned char* const c = Cur();
    if (c[0] == 0 || c[2] == 0) return;
    MoteDraw();
}

// original 0x4E6250: at radius 64 about the point, angle
// KaiserMoteA_Angles[+7] << 7 (+7 unbounded); the owner's height; +0xC 0x40,
// +5 0x10, +6 0x14, +2 on.
S30_EXPORT void __cdecl KaiserMoteA_Start(void) {
    MotePlace(AngleA(), false);
    SetLong(Cur() + 0x1C, rv::As<records::SpriteHead>(Owner())->height);
    SetWord(Cur() + 0xC, 0x40);
    Cur()[5] = 0x10;
    Cur()[6] = 0x14;
    Inc(Cur()[2]);
}

// original 0x4E62F0: +6 up by 2, the radius +0xC up by 0x20, placed; on at
// 0x200.
S30_EXPORT void __cdecl KaiserMoteA_Spread(void) {
    AddB(Cur()[6], 2);
    AddW(Cur() + 0xC, 0x20);
    MotePlace(AngleA(), true);
    if (Word(Cur() + 0xC) == 0x200) Inc(Cur()[2]);
}

// original 0x4E6390: the radius up by 0x10, placed; +5 down by 2; at 0 the
// owner's count down and the mote freed.
S30_EXPORT void __cdecl KaiserMoteA_Fade(void) {
    AddW(Cur() + 0xC, 0x10);
    MotePlace(AngleA(), true);
    AddB(Cur()[5], 0xFE);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::KaiserMote_Free);
}

// original 0x4E6430: +2 through KaiserMoteB_Phases (three entries); then
// while its delay +0x26 is not 0, the delay down; else, while +0 and +2 are
// set, its screen point and its fan.
S30_EXPORT void __cdecl KaiserMoteB_Run(void) {
    Entry("KaiserMoteB_Run", Addr(KaiserMoteB_Phases), 3, Cur()[2])();
    unsigned char* const c = Cur();
    const std::uint16_t delay = Word(c + 0x26);
    if (delay != 0) {
        SetWord(c + 0x26, (delay - 1u) & 0xFFFF);
        return;
    }
    if (c[0] == 0 || c[2] == 0) return;
    MoteDraw();
}

// original 0x4E6470: at radius 64 about the point, angle
// KaiserMoteB_Angles[+7] << 6; the owner's height, its high word up by
// KaiserMoteB_Lift[+7] << 6 (+7 unbounded); +0xC 0x40, +5 0x10, +6 8, +2 on.
S30_EXPORT void __cdecl KaiserMoteB_Start(void) {
    MotePlace(AngleB(), false);
    SetLong(Cur() + 0x1C, rv::As<records::SpriteHead>(Owner())->height);
    AddW(Cur() + 0x1E, static_cast<unsigned>(Mem(Addr(KaiserMoteB_Lift))[Cur()[7]]) << 6);
    SetWord(Cur() + 0xC, 0x40);
    Cur()[5] = 0x10;
    Cur()[6] = 8;
    Inc(Cur()[2]);
}

// original 0x4E6530: as KaiserMoteA_Spread, by KaiserMoteB_Angles.
S30_EXPORT void __cdecl KaiserMoteB_Spread(void) {
    AddB(Cur()[6], 2);
    AddW(Cur() + 0xC, 0x20);
    MotePlace(AngleB(), true);
    if (Word(Cur() + 0xC) == 0x200) Inc(Cur()[2]);
}

// original 0x4E65D0: as KaiserMoteA_Fade, by KaiserMoteB_Angles.
S30_EXPORT void __cdecl KaiserMoteB_Fade(void) {
    AddW(Cur() + 0xC, 0x10);
    MotePlace(AngleB(), true);
    AddB(Cur()[5], 0xFE);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::KaiserMote_Free);
}

// original 0x4E6670: a fan of eight gouraud triangles round the mote's screen
// point (+0x20, +0x22), radius +6 + Rand & 3; the centre's shade +5 x 14
// (kind 0) or +5 x (Rand & 3 + 4) (kind 1), the rim's 1.
S30_EXPORT void __cdecl KaiserMote_Draw(void) {
    std::uint32_t r = RandCall();
    unsigned char* c = Cur();
    SetSW(0, (r & 3) + c[6]);
    unsigned char shade;
    if (c[1] != 0) {
        r = RandCall();
        c = Cur();
        shade = static_cast<unsigned char>(((r & 3) + 4) * c[5]);
    } else {
        shade = static_cast<unsigned char>(c[5] * 14);
    }
    SetSW(0xC, Word(c + 0x20));
    SetSW(0xE, Word(c + 0x22));
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0xC));
        PutFloat(p + 0xC, SS(0xE));
        unsigned angle = (k << 9) & 0xFFF;
        SetSW(6, angle);
        int v = Sin(static_cast<short>(angle));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xC));
        v = Cos(SS(6));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xE));
        angle = ((k + 1) << 9) & 0xFFF;
        SetSW(6, angle);
        v = Sin(static_cast<short>(angle));
        PutFloat(p + 0x28, Mul12(v, SS(0)) + SS(0xC));
        v = Cos(SS(6));
        PutFloat(p + 0x2C, Mul12(v, SS(0)) + SS(0xE));
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        Commit(2, 0x34);
    }
}

// original 0x4E6830: the first of the 48 motes whose +0 bit 0 is clear gets
// it set, and its index in al; 0xFF when none is free.
S30_EXPORT unsigned char __cdecl KaiserMote_Alloc(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) != 0) continue;
        m[0] = static_cast<unsigned char>(m[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4E6880: the current mote's +0..+4 cleared.
S30_EXPORT void __cdecl KaiserMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
}

// original 0x4E68B0: the mote's screen point, as BattleActor_UpdateScreenXY
// does an actor's: (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height's high word
// / 2)) of +0x14 / +0x18 / +0x1E projected into a TILE_1 at Gfx_PacketNext
// (never committed), the float x / y truncated to +0x20 / +0x22.
S30_EXPORT void __cdecl KaiserMote_UpdateScreenXY(void) {
    const unsigned char* const c = Cur();
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((Long(c + 0x14) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(c + 0x18) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(c + 0x1E) / 2));
    v[3] = 0;
    MH_CALL(Gpu_SetTile1)(p);
    long depth, flag;
    S30_AS(Rtp1Fn, Gte_RotTransPers)(v, p + 8, &depth, &flag);
    MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    SetWord(Cur() + 0x20, Ftol16(static_cast<std::uint32_t>(Long(p + 8))));
    SetWord(Cur() + 0x22, Ftol16(static_cast<std::uint32_t>(Long(p + 0xC))));
}

void KaiserRing_Inject() {
    BOF3_INJECT(KaiserRing_Run);
    BOF3_INJECT(KaiserRing_Start);
    BOF3_INJECT(KaiserRing_Grow);
    BOF3_INJECT(KaiserRing_Fade);
    BOF3_INJECT(KaiserRing_Draw);
    BOF3_INJECT(KaiserMote_Task);
    BOF3_INJECT(KaiserMoteA_Run);
    BOF3_INJECT(KaiserMoteA_Start);
    BOF3_INJECT(KaiserMoteA_Spread);
    BOF3_INJECT(KaiserMoteA_Fade);
    BOF3_INJECT(KaiserMoteB_Run);
    BOF3_INJECT(KaiserMoteB_Start);
    BOF3_INJECT(KaiserMoteB_Spread);
    BOF3_INJECT(KaiserMoteB_Fade);
    BOF3_INJECT(KaiserMote_Draw);
    BOF3_INJECT(KaiserMote_Alloc);
    BOF3_INJECT(KaiserMote_Free);
    BOF3_INJECT(KaiserMote_UpdateScreenXY);
}

// Moved here by tools/move_funcs.py from magic_s29.cpp (docs/source-layout-round.md).
#include "game/magic/shadow_mote.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s29_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

namespace cell = magic_s29::cell;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The original's imul: 32 bits, wrapping; `sar 0xC` after it is arithmetic.
inline int MulMagicS29(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

inline int Mul12(int a, int b) { return MulMagicS29(a, b) >> 12; }

inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

inline std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

inline short S16MagicS29(const unsigned char* p) { return static_cast<short>(Word(p)); }

inline std::uint32_t AddrMagicS29(const void* p) { return Low32(p); }

inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(AddrMagicS29(p))); }

inline void IncMagicS29(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

inline void DecMagicS29(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

inline void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `fild dword` then `fstp dword`: an integer as a float.
inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// battle_items.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

inline unsigned char* ScMagicS29() { return Sprite_Current; }

inline unsigned char* OwnerMagicS29() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

inline unsigned char TargetByteMagicS29() { return Mem(::bof3::Img(at::kTarget))[0]; }

inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }

inline unsigned char* Shade(unsigned index) { return Mem(::bof3::Img(ShadowMote_Pool_addr) + (index & 0xFFu) * cell::kShadeStride); }

inline unsigned char* Cur() { return Pointer(::bof3::Img(ShadowMote_Current_addr)); }

// The scratch words and vertices, read and written in place.
inline std::uint16_t SWMagicS29(std::uint32_t a) { return Word(Mem(a)); }

inline short SSMagicS29(std::uint32_t a) { return static_cast<short>(Word(Mem(a))); }

inline unsigned char SB(std::uint32_t a) { return Mem(a)[0]; }

inline void Put(std::uint32_t a, int v) { SetWord(Mem(a), static_cast<unsigned>(v) & 0xFFFFu); }

inline const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

inline float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Callees.
int SinMagicS29(int angle) { return MH_CALL(Math_Sin)(angle); }

int CosMagicS29(int angle) { return MH_CALL(Math_Cos)(angle); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

void DrawModeMagicS29(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void CommitMagicS29(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

void LinkAtSc(unsigned size) {
    const unsigned char* const s = ScMagicS29();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    Link(ts->x, ts->z, 2, size);
}

void LinkAtCur(unsigned size) {
    const unsigned char* const r = Cur();
    Link(Long(r + 0x10), Long(r + 0x14), 2, size);
}

void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10B)(p);
}

void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Xy(p, 8), Xy(p, 0x18),
                               Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}

// Three colour bytes from the scratch words' low bytes (red, green, blue).
void Shade3(unsigned char* p) {
    p[0] = SB(::bof3::Img(cell::kW6));
    p[1] = SB(::bof3::Img(cell::kW8));
    p[2] = SB(::bof3::Img(cell::kWA));
}

void Fill3(unsigned char* p, unsigned char v) { p[0] = p[1] = p[2] = v; }

using TaskFn = void (__cdecl*)(unsigned char*);

using AllocFn = unsigned char (__cdecl*)();

void CallOwn(std::uint32_t address) { MH_WORD(Handler, address)(); }

}  // namespace

#define S29_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The orb's two rings (0x4E1D40, 0x4E1FB0): sixteen semi-transparent POLY_G4
// between an outer circle (radius R2) and an inner (radius R), 0x100 apart,
// the first two points in `outer` colours and the last two in `inner` (the
// scratch shade, or 1), each Gfx_CommitPrim(5, 0x44).
void OrbRing(bool shade_first) {
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR2))));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR2))));
    }
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        const int y = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
    }
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x1 = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        const std::uint16_t y1 = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2);
        const std::uint16_t x3 = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x18);
        Put(::bof3::Img(Prim_VertexScratch_addr), x1);
        const std::uint16_t y3 = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, y1);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, x3);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, y3);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR2))));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR2))));
        }
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
        }
        Project4(p);
        if (shade_first) {
            Shade3(p + 4);
            Shade3(p + 0x14);
            Fill3(p + 0x24, 1);
            Fill3(p + 0x34, 1);
        } else {
            Fill3(p + 4, 1);
            Fill3(p + 0x14, 1);
            Shade3(p + 0x24);
            Shade3(p + 0x34);
        }
        CommitMagicS29(5, 0x44);
    }
}

// Math_Ratan2 of the owner's point less the task's, x first then z, each an
// integer made a float (fild / fstp dword): the heading toward the owner.
int HeadingToOwner() {
    const unsigned char* const o = OwnerMagicS29();
    const unsigned char* const s = ScMagicS29();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const std::int32_t dz = Sub(rv::As<records::SpriteHead>(o)->z, ts->z);
    const std::int32_t dx = Sub(rv::As<records::SpriteHead>(o)->x, ts->x);
    return MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
}

// A shadow mote's angle: (+4 & 0xF) << 8.
std::uint16_t ShadeAngle(const unsigned char* r) { return static_cast<std::uint16_t>((r[4] & 0xFu) << 8); }

// Its point round the owner: sin / cos of the angle x `radius` (not shifted)
// plus the owner's x / z, into the current record read again after each
// call; `radius` is read after the call too (the record's +7, or the scratch
// word).
template <typename Radius> void ShadeCircle(Radius radius) {
    {
        const int s = SinMagicS29(static_cast<short>(SWMagicS29(::bof3::Img(cell::kW4))));
        unsigned char* const r = Cur();
        SetLong(r + 0x10, Add(MulMagicS29(s, radius(r)), rv::As<records::SpriteHead>(OwnerMagicS29())->x));
    }
    {
        const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        unsigned char* const r = Cur();
        SetLong(r + 0x14, Add(MulMagicS29(c, radius(r)), rv::As<records::SpriteHead>(OwnerMagicS29())->z));
    }
}

int SpreadRadius(const unsigned char* r) { return r[7]; }

// The three moving steps' common middle: the angle, the point at radius +7,
// the height +0x18 up the rise +8.
void ShadeMove() {
    Put(::bof3::Img(cell::kW4), ShadeAngle(Cur()));
    ShadeCircle(&SpreadRadius);
    unsigned char* const r = Cur();
    SetLong(r + 0x18, Add(Long(r + 0x18), Long(r + 8)));
}

}  // namespace

// original 0x4E19A0: the kind-2 task. A two-entry stack table by +1
// (ShadowBreath_Start, BattleFx_Finish), unchecked; then each live record of
// ShadowMote_Pool made ShadowMote_Current, its +0x1C the owner, and run
// through ShadowMote_Task - the owner as it was after the phase put back
// after each (the current record is left as the last).
S29_EXPORT void __cdecl ShadowBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::ShadowBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 2) PastTable("ShadowBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < cell::kShades; ++i) {
        unsigned char* const r = Shade(i);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t its = Long(r + 0x1C);
        SetPtr(Mem(::bof3::Img(ShadowMote_Current_addr)), r);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        CallOwn(bof3::addr::ShadowMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
    }
}

// original 0x4E1A10: the pool emptied (bytes 0..2 of each record); the task
// at the side's centre and its screen point; +0xB +9 0, +1 on; eight kind-1
// children 0x4E, the seekers, each its +0x80 this task, +1 2, +0xB its number
// i, +9 i + 1, counted in +0xB (the slot index unchecked); sound 0x102; row
// 26 of Gfx_ClutStrip from its source with bit 15 set (semi-transparent),
// Gfx_ClutStripDirty.
S29_EXPORT void __cdecl ShadowBreath_Start(void) {
    for (unsigned i = 0; i < cell::kShades; ++i) {
        unsigned char* const r = Shade(i);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    ScMagicS29()[0xB] = 0;
    ScMagicS29()[9] = 0;
    IncMagicS29(ScMagicS29()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const self = ScMagicS29();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[1] = 2;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(i + 1);
        IncMagicS29(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x102);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// original 0x4E1AE0: the children's kind-1 task, a jmp through
// ShadowBreathFx_Kinds (three entries: ShadowOrb_Run, ShadowGlow_Run,
// ShadowSeeker_Run) by +1, unchecked.
S29_EXPORT void __cdecl ShadowBreathFx_Task(void) {
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 3) PastTable("ShadowBreathFx_Task", phase, 3);
    DataPhase(::bof3::Img(ShadowBreathFx_Kinds_addr), phase)();
}

// original 0x4E1B00: ShadowOrb_Steps (four: MAGIC056's 0x4ADB50, Grow,
// WaitChildren, MAGIC060's 0x4B1740) by +2, unchecked; then while +0 and +2
// are set, under the actor's matrix: the rim, the band, the disc.
S29_EXPORT void __cdecl ShadowOrb_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 4) PastTable("ShadowOrb_Run", phase, 4);
    DataPhase(::bof3::Img(ShadowOrb_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::ShadowOrb_DrawRim);
    CallOwn(bof3::addr::ShadowOrb_DrawBand);
    CallOwn(bof3::addr::ShadowOrb_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E1B40: +9 up 2, +0xA up 0x18; at +9 0x10 +2 on.
S29_EXPORT void __cdecl ShadowOrb_Grow(void) {
    AddB(ScMagicS29()[9], 2);
    AddB(ScMagicS29()[0xA], 0x18);
    if (ScMagicS29()[9] == 0x10) IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1B70: +2 on once the owner's count +0xB is under 2.
S29_EXPORT void __cdecl ShadowOrb_WaitChildren(void) {
    if (OwnerMagicS29()[0xB] < 2) IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1B90: the orb's disc. Gpu_SetDrawMode(.., 0x55) and
// Gfx_CommitPrim(5, 0xC); the radius +0xA x 2, the shade +9 x 12; 32
// semi-transparent POLY_G3 of the centre and two rim points 0x100 apart, the
// centre in the shade, the rim 1; each Gfx_CommitPrim(5, 0x34).
S29_EXPORT void __cdecl ShadowOrb_DrawDisc(void) {
    DrawModeMagicS29(0x55);
    CommitMagicS29(5, 0xC);
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), s[0xA] * 2);
        Put(::bof3::Img(cell::kW6), s[9] * 12);
        Put(::bof3::Img(cell::kW8), s[9] * 12);
        Put(::bof3::Img(cell::kWA), s[9] * 12);
    }
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        const int y = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
    }
    for (int a = 0x100; a < 0x2100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
        const std::uint16_t y = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2);
        Put(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, y);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
        }
        Project3(p);
        Shade3(p + 4);
        Fill3(p + 0x14, 1);
        Fill3(p + 0x24, 1);
        CommitMagicS29(5, 0x34);
    }
}

// original 0x4E1D40: the orb's rim. Draw mode 0x35, Gfx_CommitPrim(5, 0xC);
// the inner radius +0xA x 2, the outer (Rand & 3) + +0xA x 3; the shade (+9 x
// 15, +9 x 4, +9 x 15) on the inner edge, the outer 1.
S29_EXPORT void __cdecl ShadowOrb_DrawRim(void) {
    DrawModeMagicS29(0x35);
    CommitMagicS29(5, 0xC);
    Put(::bof3::Img(cell::kR), ScMagicS29()[0xA] * 2);
    const std::uint32_t r = RandCall();
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR2), (r & 3) + s[0xA] * 3u);
        Put(::bof3::Img(cell::kW6), s[9] * 15);
        Put(::bof3::Img(cell::kW8), s[9] * 4);
        Put(::bof3::Img(cell::kWA), s[9] * 15);
    }
    OrbRing(false);
}

// original 0x4E1FB0: the orb's band. Draw mode 0x35, Gfx_CommitPrim(5, 0xC);
// the inner radius +0xA, the outer +0xA x 2; the shade (+9 x 15, +9 x 4,
// +9 x 15) on the outer edge, the inner 1.
S29_EXPORT void __cdecl ShadowOrb_DrawBand(void) {
    DrawModeMagicS29(0x35);
    CommitMagicS29(5, 0xC);
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), s[0xA]);
        Put(::bof3::Img(cell::kR2), s[0xA] * 2);
        Put(::bof3::Img(cell::kW6), s[9] * 15);
        Put(::bof3::Img(cell::kW8), s[9] * 4);
        Put(::bof3::Img(cell::kWA), s[9] * 15);
    }
    OrbRing(true);
}

// original 0x4E2210: ShadowGlow_Steps (four: Wait, MAGIC040's 0x4A5D20, Grow,
// MAGIC040's 0x4A5D50) by +2, unchecked; then while +0 and +2 are set its
// screen point and the glow drawn.
S29_EXPORT void __cdecl ShadowGlow_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 4) PastTable("ShadowGlow_Run", phase, 4);
    DataPhase(::bof3::Img(ShadowGlow_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::ShadowGlow_Draw);
}

// original 0x4E2240: +9 down; at 0 sound 0x100, the glow at the owner's
// point, +0xB 0, +9 0x14, +0xA 0, +2 on.
S29_EXPORT void __cdecl ShadowGlow_Wait(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(OwnerMagicS29())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(OwnerMagicS29())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(OwnerMagicS29())->height);
    ScMagicS29()[0xB] = 0;
    ScMagicS29()[9] = 0x14;
    ScMagicS29()[0xA] = 0;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E22C0: +9 up 2; at 0x96 +2 on.
S29_EXPORT void __cdecl ShadowGlow_Grow(void) {
    AddB(ScMagicS29()[9], 2);
    if (ScMagicS29()[9] == 0x96) IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E22E0: the glow, two semi-transparent POLY_GT4 sprites at the
// screen point (+0x2E, +0x30): half-width 0x28, the upper from y - +9 - 0x10
// to y - 0x10, the lower 0x10 below it; shade +0xA << 3 (the lower's foot 1);
// Gpu_GetTPage(0, 1, 0x340, 0x100), Gpu_GetClut(0, 0x1FA), uv 8..0x57 by 8..
// +9 + 8 and +9 + 8 .. +9 + 0x18; draw mode 0xB5 linked (2, 0xC) first, each
// sprite linked at the task (2, 0x54).
S29_EXPORT void __cdecl ShadowGlow_Draw(void) {
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), 0x28);
        Put(::bof3::Img(cell::kR2), s[9]);
        Put(::bof3::Img(cell::kW4), s[0xA] << 3);
        Put(::bof3::Img(Scratch_Swap_addr), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        Put(::bof3::Img(cell::kY), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    DrawModeMagicS29(0xB5);
    LinkAtSc(0xC);
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SSMagicS29(::bof3::Img(Scratch_Swap_addr)), y = SSMagicS29(::bof3::Img(cell::kY)), r = SSMagicS29(::bof3::Img(cell::kR)), h = SSMagicS29(::bof3::Img(cell::kR2));
        const int top = k == 0 ? y - h - 0x10 : y - 0x10;
        const int foot = k == 0 ? y - 0x10 : y;
        PutFloat(p + 8, x - r);
        PutFloat(p + 0xC, top);
        PutFloat(p + 0x1C, x + r);
        PutFloat(p + 0x20, top);
        PutFloat(p + 0x30, x - r);
        PutFloat(p + 0x34, foot);
        PutFloat(p + 0x44, x + r);
        PutFloat(p + 0x48, foot);
        {
            const unsigned t = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, t & 0xFFFFu);
        }
        {
            const unsigned c = MH_CALL(Gpu_GetClut)(0, 0x1FA);
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, c & 0xFFFFu);
        }
        const auto v = [](unsigned add) { return static_cast<unsigned char>(SB(::bof3::Img(cell::kR2)) + add); };
        if (k == 0) {
            p[0x14] = 8;
            p[0x15] = 8;
            p[0x28] = 0x57;
            p[0x29] = 8;
            p[0x3C] = 8;
            p[0x50] = 0x57;
            p[0x3D] = v(8);
            p[0x51] = v(8);
        } else {
            p[0x14] = 8;
            p[0x28] = 0x57;
            p[0x15] = v(8);
            p[0x3C] = 8;
            p[0x29] = v(8);
            p[0x50] = 0x57;
            p[0x3D] = v(0x18);
            p[0x51] = v(0x18);
        }
        const unsigned char g = SB(::bof3::Img(cell::kW4));
        Fill3(p + 4, g);
        Fill3(p + 0x18, g);
        Fill3(p + 0x2C, k == 0 ? g : 1);
        Fill3(p + 0x40, k == 0 ? g : 1);
        LinkAtSc(0x54);
    }
}

// original 0x4E26A0: ShadowSeeker_Steps (four) by +2, unchecked; then while
// +0 and +2 are set its screen point and the seeker drawn.
S29_EXPORT void __cdecl ShadowSeeker_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 4) PastTable("ShadowSeeker_Run", phase, 4);
    DataPhase(::bof3::Img(ShadowSeeker_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::ShadowSeeker_Draw);
}

// original 0x4E26D0: +9 down; at 0 the seeker starts at the acting actor's
// sprite (0x904B3C): its direction byte and point; the offset (0x8000, 0)
// turned by the direction (0x446770) and added, the height up 0x800000; its
// heading +0xC toward the owner; +9 0, +2 on.
S29_EXPORT void __cdecl ShadowSeeker_Launch(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    ScMagicS29()[8] = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)))->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)))->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)))->height);
    rv::Current<records::BattleTask>()->step_x = 0x8000;
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_AT(TaskFn, magic_s29::kTurnByFacing)(ScMagicS29());
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = Add(ts->x, ts->step_x);
        ts->z = Add(ts->z, ts->step_z);
        rv::Store(ts->height, Add(ts->height, 0x800000));
    }
    const int heading = HeadingToOwner();
    rv::Current<records::BattleTask>()->step_x = heading;
    ScMagicS29()[9] = 0;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E27E0: +9 up 4 while under 0x10; a step toward the owner
// (MagicFx_StepToward, speed 0x60); the old heading to +0x10, the new to
// +0xC; +2 on when MagicFx_NearSprite(owner, 0xC000), or when the turn
// |(+0x10 & 0xFFF) - (+0xC & 0xFFF)| (kept in +0x10) lies strictly between
// 0x600 and 0xA00 - it has passed the owner.
S29_EXPORT void __cdecl ShadowSeeker_Home(void) {
    {
        unsigned char* const s = ScMagicS29();
        if (s[9] < 0x10) AddB(s[9], 4);
    }
    MH_CALL(MagicFx_StepToward)(OwnerMagicS29(), 0x60);
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = ts->step_x;
    }
    const int heading = HeadingToOwner();
    rv::Current<records::BattleTask>()->step_x = heading;
    const int near = MH_CALL(MagicFx_NearSprite)(OwnerMagicS29(), 0xC000);
    unsigned char* const s = ScMagicS29();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (near != 0) {
        IncMagicS29(s[2]);
        return;
    }
    ts->step_z = Sub(ts->step_z & 0xFFF, ts->step_x & 0xFFF);
    if (ts->step_z < 0) ts->step_z = Sub(0, ts->step_z);
    const std::int32_t turn = ts->step_z;
    if (turn > 0x600 && turn < 0xA00) IncMagicS29(s[2]);
}

// original 0x4E28C0: a seeker with +0xB 0 (the first) bursts, any other only
// steps on. The burst: sound 0x101; two kind-1 children 0x4E of the owner -
// the orb (+1 0) and the glow (+1 1, +9 0x2E) - counted in the owner's +0xB;
// 64 motes (ShadowMote_Alloc, the index unchecked), each its +0x1C the owner,
// +1 0, +4 its number i, delay +5 (Rand & 0xF) + (i & 1) x 16 + 0x18, counted
// in the owner's +0xB; the targets' flag 0x10; +2 on.
S29_EXPORT void __cdecl ShadowSeeker_Burst(void) {
    if (ScMagicS29()[0xB] != 0) {
        IncMagicS29(ScMagicS29()[2]);
        return;
    }
    MH_CALL(Sound_PlayById)(0x101);
    {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const o = OwnerMagicS29();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, o);
        child[1] = 0;
        IncMagicS29(o[0xB]);
    }
    {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const o = OwnerMagicS29();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, o);
        child[1] = 1;
        child[9] = 0x2E;
        IncMagicS29(o[0xB]);
    }
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned index = MH_AT(AllocFn, bof3::addr::ShadowMote_Alloc)();
        unsigned char* const r = Shade(index);
        SetLong(r + 0x1C, Long(Mem(::bof3::Img(BattleE2_Owner_addr))));
        r[1] = 0;
        r[4] = static_cast<unsigned char>(i);
        const std::uint32_t n = RandCall();
        r[5] = static_cast<unsigned char>((n & 0xF) + ((i & 1) << 4) + 0x18);
        IncMagicS29(OwnerMagicS29()[0xB]);
    }
    MH_CALL(Battle_SetTargetFlags)(TargetByteMagicS29(), 0x10);
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E29E0: +9 down 2; once negative (as a signed byte) the owner's
// +0xB down and the task freed.
S29_EXPORT void __cdecl ShadowSeeker_Fade(void) {
    AddB(ScMagicS29()[9], 0xFE);
    if (static_cast<signed char>(ScMagicS29()[9]) >= 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E2A10: the seeker in screen space. Draw mode 0x35,
// Gfx_CommitPrim(3, 0xC); sixteen semi-transparent POLY_G3 fanned round the
// screen point (+0x2E, +0x30), radius 0x18, 0x100 apart (the rim sin / cos x
// 0x18 >> 12 plus the point, as integers); the centre ShadowSeeker_Colours
// [+0xB] (red, blue) x +9 and +9 green, the rim 1; each Gfx_CommitPrim(3,
// 0x34). The table's index +0xB is unchecked.
S29_EXPORT void __cdecl ShadowSeeker_Draw(void) {
    DrawModeMagicS29(0x35);
    CommitMagicS29(3, 0xC);
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kW4), 0);
        Put(::bof3::Img(cell::kR), 0x18);
        Put(::bof3::Img(Scratch_Swap_addr), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        Put(::bof3::Img(cell::kY), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        const unsigned char* const c = Mem(::bof3::Img(ShadowSeeker_Colours_addr) + s[0xB] * 2u);
        Put(::bof3::Img(cell::kW6), c[0] * s[9]);
        Put(::bof3::Img(cell::kW8), s[9]);
        Put(::bof3::Img(cell::kWA), c[1] * s[9]);
    }
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SSMagicS29(::bof3::Img(Scratch_Swap_addr)));
        PutFloat(p + 0xC, SSMagicS29(::bof3::Img(cell::kY)));
        {
            const int s = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
            PutFloat(p + 0x18, Add(Mul12(s, SSMagicS29(::bof3::Img(cell::kR))), SSMagicS29(::bof3::Img(Scratch_Swap_addr))));
        }
        {
            const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
            PutFloat(p + 0x1C, Add(Mul12(c, SSMagicS29(::bof3::Img(cell::kR))), SSMagicS29(::bof3::Img(cell::kY))));
        }
        Put(::bof3::Img(cell::kW4), (i + 1) << 8);
        {
            const int s = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
            PutFloat(p + 0x28, Add(Mul12(s, SSMagicS29(::bof3::Img(cell::kR))), SSMagicS29(::bof3::Img(Scratch_Swap_addr))));
        }
        {
            const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
            PutFloat(p + 0x2C, Add(Mul12(c, SSMagicS29(::bof3::Img(cell::kR))), SSMagicS29(::bof3::Img(cell::kY))));
        }
        Shade3(p + 4);
        Fill3(p + 0x14, 1);
        Fill3(p + 0x24, 1);
        CommitMagicS29(3, 0x34);
    }
}

// original 0x4E2BF0: a mote of the pool, ShadowMote_Current: a jmp through
// ShadowMote_TaskTable (one entry, ShadowMote_Run) by its +1, unchecked.
S29_EXPORT void __cdecl ShadowMote_Task(void) {
    const unsigned phase = Cur()[1];
    if (phase >= 1) PastTable("ShadowMote_Task", phase, 1);
    DataPhase(::bof3::Img(ShadowMote_TaskTable_addr), phase)();
}

// original 0x4E2C10: ShadowMote_Steps (four) by the current record's +2,
// unchecked; then while its +0 and +2 are set, its screen point and the
// sprite drawn.
S29_EXPORT void __cdecl ShadowMote_Run(void) {
    const unsigned phase = Cur()[2];
    if (phase >= 4) PastTable("ShadowMote_Run", phase, 4);
    DataPhase(::bof3::Img(ShadowMote_Steps_addr), phase)();
    const unsigned char* const r = Cur();
    if (r[0] == 0 || r[2] == 0) return;
    CallOwn(bof3::addr::ShadowMote_Project);
    CallOwn(bof3::addr::ShadowMote_Draw);
}

// original 0x4E2C40: the current record's +5 down; at 0 it starts: angle
// (+4 & 0xF) << 8, radius (+4 >> 4) x 8 + 8; its point round the owner at
// that radius, the owner's height + 0x800000; rise +8 0, +5 +6 0, +7 the
// radius, +2 on.
S29_EXPORT void __cdecl ShadowMote_Wait(void) {
    DecMagicS29(Cur()[5]);
    const unsigned char* const r = Cur();
    if (r[5] != 0) return;
    Put(::bof3::Img(cell::kW4), ShadeAngle(r));
    Put(::bof3::Img(cell::kR), (r[4] >> 4) * 8 + 8);
    ShadeCircle([](const unsigned char*) { return static_cast<int>(SSMagicS29(::bof3::Img(cell::kR))); });
    SetLong(Cur() + 0x18, Add(rv::As<records::SpriteHead>(OwnerMagicS29())->height, 0x800000));
    SetLong(Cur() + 8, 0);
    Cur()[5] = 0;
    Cur()[6] = 0;
    Cur()[7] = SB(::bof3::Img(cell::kR));
    IncMagicS29(Cur()[2]);
}

// original 0x4E2D30: on odd frames the size +5 and the radius +7 up; the
// point round the owner at +7, the height up +8; +6 up, at 0x10 +2 on.
S29_EXPORT void __cdecl ShadowMote_Spread(void) {
    if (Frame_Counter & 1) {
        IncMagicS29(Cur()[5]);
        IncMagicS29(Cur()[7]);
    }
    ShadeMove();
    IncMagicS29(Cur()[6]);
    if (Cur()[6] == 0x10) IncMagicS29(Cur()[2]);
}

// original 0x4E2DF0: on odd frames the radius +7 up; the rise +8 up 0x80000;
// the point and height as Spread; +2 on once the rise is 0x800000 or more
// (signed).
S29_EXPORT void __cdecl ShadowMote_Lift(void) {
    if (Frame_Counter & 1) IncMagicS29(Cur()[7]);
    {
        unsigned char* const r = Cur();
        SetLong(r + 8, Add(Long(r + 8), 0x80000));
    }
    ShadeMove();
    if (Long(Cur() + 8) >= 0x800000) IncMagicS29(Cur()[2]);
}

// original 0x4E2EA0: on odd frames the radius +7 up; the point and height as
// Spread; +6 down, at 0 the owner's +0xB down and the record freed
// (ShadowMote_Free).
S29_EXPORT void __cdecl ShadowMote_Fade(void) {
    if (Frame_Counter & 1) IncMagicS29(Cur()[7]);
    ShadeMove();
    DecMagicS29(Cur()[6]);
    if (Cur()[6] != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    CallOwn(bof3::addr::ShadowMote_Free);
}

// original 0x4E2F60: the mote as a semi-transparent POLY_FT4 in screen space
// at its point (+0xC, +0xE): half-width +5, height +5 x 4 above it, shade +6
// x 6; Gpu_GetTPage(0, 1, 0x340, 0x100), Gpu_GetClut(0, 0x1FA), uv 0x58..0x68
// by 8..0x48; draw mode 0xB5 linked at the record (2, 0xC), the sprite
// linked (2, 0x48).
S29_EXPORT void __cdecl ShadowMote_Draw(void) {
    {
        const unsigned char* const r = Cur();
        Put(::bof3::Img(cell::kR), r[5]);
        Put(::bof3::Img(cell::kR2), r[5] * 4);
        Put(::bof3::Img(cell::kW4), r[6] * 6);
        Put(::bof3::Img(Scratch_Swap_addr), Word(r + 0xC));
        Put(::bof3::Img(cell::kY), Word(r + 0xE));
    }
    DrawModeMagicS29(0xB5);
    LinkAtCur(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const int x = SSMagicS29(::bof3::Img(Scratch_Swap_addr)), y = SSMagicS29(::bof3::Img(cell::kY)), r = SSMagicS29(::bof3::Img(cell::kR)), h = SSMagicS29(::bof3::Img(cell::kR2));
        PutFloat(p + 8, x - r);
        PutFloat(p + 0xC, y - h);
        PutFloat(p + 0x18, x + r);
        PutFloat(p + 0x1C, y - h);
        PutFloat(p + 0x28, x - r);
        PutFloat(p + 0x2C, y);
        PutFloat(p + 0x38, x + r);
        PutFloat(p + 0x3C, y);
    }
    {
        const unsigned t = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, t & 0xFFFFu);
    }
    {
        const unsigned c = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, c & 0xFFFFu);
    }
    p[0x15] = 8;
    p[0x25] = 8;
    p[0x14] = 0x58;
    p[0x24] = 0x68;
    p[0x34] = 0x58;
    p[0x35] = 0x48;
    p[0x44] = 0x68;
    p[0x45] = 0x48;
    Fill3(p + 4, SB(::bof3::Img(cell::kW4)));
    LinkAtCur(0x48);
}

// original 0x4E3140: the first free record of ShadowMote_Pool (+0 bit 0
// clear) taken (bit 0 set); its index, or 0xFF when all 128 are in use.
S29_EXPORT unsigned char __cdecl ShadowMote_Alloc(void) {
    for (unsigned n = 0; n < cell::kShades; ++n) {
        unsigned char* const r = Shade(n);
        if (r[0] & 1) continue;
        r[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

// original 0x4E3190: the current record's bytes 0..3 cleared.
S29_EXPORT void __cdecl ShadowMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
}

// original 0x4E31C0: the current record's screen point, as
// BattleActor_UpdateScreenXY's for a task: (x >> 9 - 0x4000, z >> 9 - 0x4000,
// -(height word +0x1A / 2)) projected into a TILE_1 at Gfx_PacketNext -
// never committed, used for its vertex - and the float x / y truncated (the
// CRT's _ftol) to +0xC / +0xE.
S29_EXPORT void __cdecl ShadowMote_Project(void) {
    const unsigned char* const r = Cur();
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((Long(r + 0x10) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(r + 0x14) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16MagicS29(r + 0x1A) / 2));
    v[3] = 0;
    MH_CALL(Gpu_SetTile1)(p);
    long depth;
    MH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), &depth);
    MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    SetWord(Cur() + 0xC, Ftol16(static_cast<std::uint32_t>(Long(p + 8))));
    SetWord(Cur() + 0xE, Ftol16(static_cast<std::uint32_t>(Long(p + 0xC))));
}

void ShadowMote_Inject() {
    BOF3_INJECT(ShadowBreath_Task);
    BOF3_INJECT(ShadowBreath_Start);
    BOF3_INJECT(ShadowBreathFx_Task);
    BOF3_INJECT(ShadowOrb_Run);
    BOF3_INJECT(ShadowOrb_Grow);
    BOF3_INJECT(ShadowOrb_WaitChildren);
    BOF3_INJECT(ShadowOrb_DrawDisc);
    BOF3_INJECT(ShadowOrb_DrawRim);
    BOF3_INJECT(ShadowOrb_DrawBand);
    BOF3_INJECT(ShadowGlow_Run);
    BOF3_INJECT(ShadowGlow_Wait);
    BOF3_INJECT(ShadowGlow_Grow);
    BOF3_INJECT(ShadowGlow_Draw);
    BOF3_INJECT(ShadowSeeker_Run);
    BOF3_INJECT(ShadowSeeker_Launch);
    BOF3_INJECT(ShadowSeeker_Home);
    BOF3_INJECT(ShadowSeeker_Burst);
    BOF3_INJECT(ShadowSeeker_Fade);
    BOF3_INJECT(ShadowSeeker_Draw);
    BOF3_INJECT(ShadowMote_Task);
    BOF3_INJECT(ShadowMote_Run);
    BOF3_INJECT(ShadowMote_Wait);
    BOF3_INJECT(ShadowMote_Spread);
    BOF3_INJECT(ShadowMote_Lift);
    BOF3_INJECT(ShadowMote_Fade);
    BOF3_INJECT(ShadowMote_Draw);
    BOF3_INJECT(ShadowMote_Alloc);
    BOF3_INJECT(ShadowMote_Free);
    BOF3_INJECT(ShadowMote_Project);
}

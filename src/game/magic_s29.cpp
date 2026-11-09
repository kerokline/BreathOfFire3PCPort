// Spell group S29 of round nine (docs/magic_s29.md): two BMAGIC overlays of
// Magic_Rows compiled into the exe, 49 functions.
//
//   MAGIC125, row 125 (read one id down: DivineBreath) 0x4E0910..0x4E1996: a
//     kind-2 task with two kind-1 children (parameter 0x5C) - a beam that
//     drops from above the side's centre and widens (a disc of POLY_G3 and a
//     wall of POLY_G4), and a burst that closes in, sets the targets' 0x10
//     flag and sheds 64 motes into a pool of its own (0x69FC30, task-shaped
//     records) - each mote flying out and up, drawn as a screen-space star of
//     POLY_G3 and a ring of POLY_G4.
//   MAGIC126, row 127 (read one id down: ShadowBreath) 0x4E19A0..0x4E3254: a
//     kind-2 task with eight kind-1 seekers (parameter 0x4E) launched from the
//     acting actor, homing on the side's centre; the first to arrive makes an
//     orb (three rings of POLY_G3 / POLY_G4), a glow (two POLY_GT4 sprites),
//     sets the targets' 0x10 flag, and sheds 64 motes into a pool of 0x20-byte
//     records (0x6A1D30) that spread, lift and fade as POLY_FT4 sprites.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a direct call to another function of the group goes by its address, which
// in the game is the jmp Inject put there to ours.
//
// No divergence: each function is a faithful replacement, except that a
// dispatch past its table - a stack table's or a .data table's - aborts where
// the original would call through whatever follows (docs/magic_fx_reached.md
// section 3, the precedent). Where the original reads memory again after a
// call (Sprite_Current, the owner, the pool's current record, the scratch
// words), ours reads it again; where it holds a pointer across a call, ours
// holds it.
#include "game/magic_s29.h"

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
namespace cell = magic_s29::cell;
namespace tbl = magic_s29::tbl;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- small helpers ---------------------------------------------------------

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

// `fild dword` then `fstp dword`: an integer as a float.
inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

inline unsigned char* ScMagicS29() { return Sprite_Current; }
inline unsigned char* OwnerMagicS29() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
inline unsigned char TargetByteMagicS29() { return Mem(::bof3::Img(at::kTarget))[0]; }
inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }
inline unsigned char* Mote(unsigned index) { return Mem(::bof3::Img(cell::kMotePool) + (index & 0xFFu) * cell::kMoteStride); }

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

using AllocFn = unsigned char (__cdecl*)();
void CallOwn(std::uint32_t address) { MH_WORD(Handler, address)(); }

}  // namespace

#define S29_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC125 (row 125, DivineBreath read one id down)

// original 0x4E0910: the kind-2 task. A two-entry stack table by +1
// (DivineBreath_Start, BattleFx_Finish), unchecked; then each live record of
// DivineMote_Pool run as Sprite_Current, its +0x80 the owner, through
// DivineMote_Task - Sprite_Current and the owner as they were after the
// phase put back after each.
S29_EXPORT void __cdecl DivineBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::DivineBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 2) PastTable("DivineBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const saved = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        CallOwn(bof3::addr::DivineMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = saved;
    }
}

// original 0x4E0990: the pool emptied (bytes 0..2 of each record); the task
// at the side's centre (MagicFx_CenterOnSide); +0xB 0, +1 on; two kind-1
// children 0x5C, each its +0x80 this task: the beam (+1 0, +9 1) and the
// burst (+1 1, +9 9), +0xB counting them (the slot index unchecked); sound
// 0x100.
S29_EXPORT void __cdecl DivineBreath_Start(void) {
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    ScMagicS29()[0xB] = 0;
    IncMagicS29(ScMagicS29()[1]);
    static constexpr unsigned char kKind[2] = {0, 1};
    static constexpr unsigned char kDelay[2] = {1, 9};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned slot = NewTask(0x5C);
        unsigned char* const self = ScMagicS29();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[1] = kKind[k];
        child[9] = kDelay[k];
        IncMagicS29(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4E0A60: the children's kind-1 task, a jmp through
// DivineBreathFx_Kinds (two entries: DivineBeam_Run, DivineBurst_Run) by +1,
// unchecked.
S29_EXPORT void __cdecl DivineBreathFx_Task(void) {
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 2) PastTable("DivineBreathFx_Task", phase, 2);
    DataPhase(::bof3::Img(DivineBreathFx_Kinds_addr), phase)();
}

// original 0x4E0A80: DivineBeam_Steps (three) by +2, unchecked; then while
// +0 and +2 are set: on even frames three new colour bytes +0x5D..+0x5F
// ((Rand & 3) + 4 each), the actor's matrix pushed, the disc and the wall
// drawn, the matrix popped.
S29_EXPORT void __cdecl DivineBeam_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 3) PastTable("DivineBeam_Run", phase, 3);
    DataPhase(::bof3::Img(DivineBeam_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    if ((Frame_Counter & 1) == 0) {
        for (unsigned k = 0x5D; k <= 0x5F; ++k) {
            const std::uint32_t r = RandCall();
            ScMagicS29()[k] = static_cast<unsigned char>((r & 3) + 4);
        }
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawDisc);
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E0B00: +9 down; at 0 the beam starts at the owner's point, its
// height 0x4000000 above; three colour bytes (Rand & 3) + 4; radius +0x14
// 0x100, +9 0x10, +0xA 8, +2 on.
S29_EXPORT void __cdecl DivineBeam_Wait(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(OwnerMagicS29())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(OwnerMagicS29())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::As<records::SpriteHead>(OwnerMagicS29())->height, 0x4000000));
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        ScMagicS29()[k] = static_cast<unsigned char>((r & 3) + 4);
    }
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    ScMagicS29()[9] = 0x10;
    ScMagicS29()[0xA] = 8;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E0BB0: the height down 0x800000; +0xA down, at 0 +2 on.
S29_EXPORT void __cdecl DivineBeam_Descend(void) {
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(0xFF800000u)));
    DecMagicS29(ScMagicS29()[0xA]);
    if (ScMagicS29()[0xA] == 0) IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E0BE0: the radius +0x14 up 0x20; +9 down; at 0 the owner's
// +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBeam_Widen(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_y, Add(rv::Current<records::BattleTask>()->step_y, 0x20));
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The colours both beam draws take: word 0x903856 / 58 / 5A = the signed
// colour byte +0x5D / +0x5E / +0x5F times the brightness +9.
void BeamColours() {
    const unsigned char* const s = ScMagicS29();
    Put(::bof3::Img(cell::kW6), MulMagicS29(static_cast<signed char>(s[0x5D]), s[9]));
    Put(::bof3::Img(cell::kW8), MulMagicS29(static_cast<signed char>(s[0x5E]), s[9]));
    Put(::bof3::Img(cell::kWA), MulMagicS29(static_cast<signed char>(s[0x5F]), s[9]));
}

}  // namespace

// original 0x4E0C20: the beam's disc. Gpu_SetDrawMode(.., 0x35) and
// Gfx_CommitPrim(5, 0xC); the radius the word +0x14; sixteen semi-transparent
// POLY_G3 of the centre and two rim points 0x100 apart (sin / cos x radius
// >> 12), the centre in the colours, the rim 0x10 grey, each
// Gfx_CommitPrim(5, 0x34); then the draw mode 0x15.
S29_EXPORT void __cdecl DivineBeam_DrawDisc(void) {
    DrawModeMagicS29(0x35);
    CommitMagicS29(5, 0xC);
    Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_y));
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
    }
    BeamColours();
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2);
        const std::uint16_t x = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
        Put(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, y);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        const int c = CosMagicS29(a);
        const int ny = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        unsigned char* const p = Gfx_PacketNext;
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, ny);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Project3(p);
        Shade3(p + 4);
        Fill3(p + 0x14, 0x10);
        Fill3(p + 0x24, 0x10);
        CommitMagicS29(5, 0x34);
    }
    DrawModeMagicS29(0x15);
    CommitMagicS29(5, 0xC);
}

// original 0x4E0E00: the beam's wall. The radius the word +0x14; 32
// semi-transparent POLY_G4, each between two rim points 0x80 apart at height
// -0x300 and the same two at 0, the top dark (1), the foot in the colours:
// each after Gpu_SetDrawMode(.., 0x35) linked by MapView_LinkPrimAt at the
// task's point plus the new rim point << 9 (0, 0xC), and again (0, 0x44).
S29_EXPORT void __cdecl DivineBeam_DrawWall(void) {
    Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_y));
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
    }
    BeamColours();
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        const std::uint16_t y = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2);
        const std::uint16_t x = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        Put(::bof3::Img(Prim_VertexScratch_addr), x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, -0x300);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        const int c = CosMagicS29(a);
        const int ny = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        const std::uint16_t nx = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, -0x300);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, nx);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        const unsigned char* const s = ScMagicS29();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, ny);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, ny);
        const std::int32_t lx = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(nx)) << 9),
                                    ts->x);
        const std::int32_t lz = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(ny)) << 9),
                                    ts->z);
        DrawModeMagicS29(0x35);
        Link(lx, lz, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Fill3(p + 4, 1);
        Fill3(p + 0x14, 1);
        Shade3(p + 0x24);
        Shade3(p + 0x34);
        Project4(p);
        Link(lx, lz, 0, 0x44);
    }
}

// original 0x4E1030: DivineBurst_Steps (three) by +2, unchecked; then while
// +0 and +2 are set the wall drawn under the actor's matrix.
S29_EXPORT void __cdecl DivineBurst_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 3) PastTable("DivineBurst_Run", phase, 3);
    DataPhase(::bof3::Img(DivineBurst_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E1070: +9 down; at 0 the burst at the owner's point, colour
// bytes 0xF, radius 0x100, its step +0x20 0x21, +9 0x10, +0xA 0x1E, +2 on.
S29_EXPORT void __cdecl DivineBurst_Wait(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(OwnerMagicS29())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(OwnerMagicS29())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(OwnerMagicS29())->height);
    ScMagicS29()[0x5D] = 0xF;
    ScMagicS29()[0x5E] = 0xF;
    ScMagicS29()[0x5F] = 0xF;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0x21);
    ScMagicS29()[9] = 0x10;
    ScMagicS29()[0xA] = 0x1E;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1110: the step +0x20 down 2 unless it is 1; the radius +0x14
// less it; once the radius is under 0x30 (signed): the targets' flag 0x10,
// sound 0x101, 64 motes (DivineMote_Alloc, the index unchecked), each its
// +0x80 the owner, +1 0, +0xB its number i, +9 (Rand & 7) + 8 (i / 6) + 1,
// counted in the owner's +0xB; +2 on.
S29_EXPORT void __cdecl DivineBurst_Shrink(void) {
    {
        unsigned char* const s = ScMagicS29();
        if (rv::As<records::BattleTask>(s)->work_20 != 1) rv::Store(rv::As<records::BattleTask>(s)->work_20, Sub(rv::As<records::BattleTask>(s)->work_20, 2));
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Sub(rv::As<records::BattleTask>(s)->step_y, rv::As<records::BattleTask>(s)->work_20));
    }
    if (rv::Current<records::BattleTask>()->step_y >= 0x30) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByteMagicS29(), 0x10);
    MH_CALL(Sound_PlayById)(0x101);
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned slot = MH_AT(AllocFn, bof3::addr::DivineMote_Alloc)();
        unsigned char* const m = Mote(slot);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
        m[1] = 0;
        m[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        m[9] = static_cast<unsigned char>((r & 7) + (i / 6) * 8 + 1);
        IncMagicS29(OwnerMagicS29()[0xB]);
    }
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E11F0: the radius down one while over 8 (signed); on odd frames
// +9 down, at 0 the owner's +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBurst_Fade(void) {
    {
        unsigned char* const s = ScMagicS29();
        if (rv::As<records::BattleTask>(s)->step_y > 8) rv::Store(rv::As<records::BattleTask>(s)->step_y, Sub(rv::As<records::BattleTask>(s)->step_y, 1));
    }
    if ((Frame_Counter & 1) == 0) return;
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E1240: a mote, a jmp through DivineMote_TaskTable (one entry,
// DivineMote_Run) by +1, unchecked.
S29_EXPORT void __cdecl DivineMote_Task(void) {
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 1) PastTable("DivineMote_Task", phase, 1);
    DataPhase(::bof3::Img(DivineMote_TaskTable_addr), phase)();
}

// original 0x4E1260: DivineMote_Steps (two) by +2, unchecked; then while +0
// and +2 are set: draw mode 0x35 linked at the mote (2, 0xC), its screen
// point (BattleActor_UpdateScreenXY), its star and ring, draw mode 0x15
// linked the same.
S29_EXPORT void __cdecl DivineMote_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 2) PastTable("DivineMote_Run", phase, 2);
    DataPhase(::bof3::Img(DivineMote_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    DrawModeMagicS29(0x35);
    LinkAtSc(0xC);
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::DivineMote_DrawStar);
    CallOwn(bof3::addr::DivineMote_DrawRing);
    DrawModeMagicS29(0x15);
    LinkAtSc(0xC);
}

namespace {

// A mote's angle: ((+0xB & 7) x 8 + +9) << 6, a word.
std::uint16_t MoteAngleMagicS29(const unsigned char* s) {
    return static_cast<std::uint16_t>(((s[0xB] & 7u) * 8 + s[9]) << 6);
}

}  // namespace

// original 0x4E12F0: +9 down; at 0 the mote starts: distance +0xC 0x5000, its
// step +0x18 0x200, rise +0x14 0x80000, its step +0x20 0x40000; the point
// the owner's plus sin / cos of its angle x the distance >> 12, the owner's
// height; colour bytes (Rand & 7) + 5; +9 0, +0xA 0x18, +2 on.
S29_EXPORT void __cdecl DivineMote_Launch(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = 0x5000;
        ts->work_18 = 0x200;
        rv::Store(ts->step_y, 0x80000);
        rv::Store(ts->work_20, 0x40000);
        Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(ts->step_x));
        Put(::bof3::Img(cell::kW4), MoteAngleMagicS29(s));
    }
    {
        const int sn = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        const std::int32_t v = Add(Mul12(sn, SSMagicS29(::bof3::Img(cell::kR))), rv::As<records::SpriteHead>(OwnerMagicS29())->x);
        rv::Current<records::BattleTask>()->x = v;
    }
    {
        const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        const std::int32_t v = Add(Mul12(c, SSMagicS29(::bof3::Img(cell::kR))), rv::As<records::SpriteHead>(OwnerMagicS29())->z);
        rv::Current<records::BattleTask>()->z = v;
    }
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(OwnerMagicS29())->height);
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        ScMagicS29()[k] = static_cast<unsigned char>((r & 7) + 5);
    }
    ScMagicS29()[9] = 0;
    ScMagicS29()[0xA] = 0x18;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1430: +9 up; the distance +0xC less its step +0x18, the rise
// +0x14 plus its step +0x20; the point moved by sin / cos of the angle x the
// distance >> 12 (each into the record the pointer held before the call), the
// height up the rise; at distance 0 the owner's +0xB down and the record
// freed (0x4F6290, MAGIC219's).
S29_EXPORT void __cdecl DivineMote_Fly(void) {
    IncMagicS29(ScMagicS29()[9]);
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = Sub(ts->step_x, ts->work_18);
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Add(rv::As<records::BattleTask>(s)->step_y, rv::As<records::BattleTask>(s)->work_20));
    }
    unsigned char* x = nullptr;
    {
        unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
        x = s + 0x34;
        Put(::bof3::Img(cell::kW4), MoteAngleMagicS29(s));
    }
    {
        const int sn = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        SetLong(x, Add(Long(x), Mul12(sn, SSMagicS29(::bof3::Img(cell::kR)))));
    }
    unsigned char* const z = ScMagicS29() + 0x38;
    {
        const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        SetLong(z, Add(Long(z), Mul12(c, SSMagicS29(::bof3::Img(cell::kR)))));
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->height, Add(rv::As<records::BattleTask>(s)->height, rv::As<records::BattleTask>(s)->step_y));
    }
    if (rv::Current<records::BattleTask>()->step_x != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_AT(Handler, bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4E1510: the mote's star in screen space: radius +0xA; eight
// semi-transparent POLY_G3 of the screen point (+0x2E, +0x30) and two rim
// points 0x200 apart (sin / cos x radius >> 12 plus the point, as words),
// the centre 0xF0 grey, the rim the colour bytes << 4; each linked at the
// mote (2, 0x34).
S29_EXPORT void __cdecl DivineMote_DrawStar(void) {
    Put(::bof3::Img(cell::kR), ScMagicS29()[0xA]);
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    }
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kW6), MulMagicS29(static_cast<signed char>(s[0x5D]), 16));
        Put(::bof3::Img(cell::kW8), MulMagicS29(static_cast<signed char>(s[0x5E]), 16));
        Put(::bof3::Img(cell::kWA), MulMagicS29(static_cast<signed char>(s[0x5F]), 16));
    }
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned char* const s = ScMagicS29();
            PutFloat(p + 8, S16MagicS29(s + 0x2E));
            PutFloat(p + 0xC, S16MagicS29(s + 0x30));
            PutFloat(p + 0x18, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
            PutFloat(p + 0x1C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        }
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x28, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            Fill3(p + 4, 0xF0);
            PutFloat(p + 0x2C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
            Shade3(p + 0x14);
            Shade3(p + 0x24);
        }
        LinkAtSc(0x34);
    }
}

// original 0x4E16E0: the mote's ring in screen space: radii +0xA and twice
// it; eight semi-transparent POLY_G4 between the two circles, 0x200 apart,
// the outer edge dark (1), the inner in the colour words the star left; each
// linked at the mote (2, 0x44).
S29_EXPORT void __cdecl DivineMote_DrawRing(void) {
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), s[0xA]);
        Put(::bof3::Img(cell::kR2), s[0xA] * 2);
    }
    // the rim points: v0 on the outer circle, v1 on the inner, at angle a
    const auto rim = [](int a, std::uint32_t radius, std::uint32_t v) {
        {
            const int s = SinMagicS29(a);
            Put(v, Mul12(s, SSMagicS29(radius)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
        }
        const int c = CosMagicS29(a);
        Put(v + 2, Mul12(c, SSMagicS29(radius)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    };
    rim(0, ::bof3::Img(cell::kR2), ::bof3::Img(Prim_VertexScratch_addr));
    rim(0, ::bof3::Img(cell::kR), ::bof3::Img(cell::kV1));
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        PutFloat(p + 0xC, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        PutFloat(p + 0x28, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        PutFloat(p + 0x2C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR2))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x18, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR2))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            PutFloat(p + 0x1C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        }
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x38, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            Fill3(p + 4, 1);
            Fill3(p + 0x14, 1);
            PutFloat(p + 0x3C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            Shade3(p + 0x24);
            Shade3(p + 0x34);
        }
        LinkAtSc(0x44);
    }
}

// original 0x4E1940: the first free record of DivineMote_Pool (+0 bit 0
// clear) taken (bit 0 set); its index, or 0xFF when all 64 are in use.
S29_EXPORT unsigned char __cdecl DivineMote_Alloc(void) {
    for (unsigned n = 0; n < cell::kMotes; ++n) {
        unsigned char* const m = Mote(n);
        if (m[0] & 1) continue;
        m[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC126 (row 127, ShadowBreath read one id down)

void MagicS29_Inject() {
    if (bof3::WantsShadow("magic_s29")) magic_s29::SelfTest();
    BOF3_INJECT(DivineBreath_Task);
    BOF3_INJECT(DivineBreath_Start);
    BOF3_INJECT(DivineBreathFx_Task);
    BOF3_INJECT(DivineBeam_Run);
    BOF3_INJECT(DivineBeam_Wait);
    BOF3_INJECT(DivineBeam_Descend);
    BOF3_INJECT(DivineBeam_Widen);
    BOF3_INJECT(DivineBeam_DrawDisc);
    BOF3_INJECT(DivineBeam_DrawWall);
    BOF3_INJECT(DivineBurst_Run);
    BOF3_INJECT(DivineBurst_Wait);
    BOF3_INJECT(DivineBurst_Shrink);
    BOF3_INJECT(DivineBurst_Fade);
    BOF3_INJECT(DivineMote_Task);
    BOF3_INJECT(DivineMote_Run);
    BOF3_INJECT(DivineMote_Launch);
    BOF3_INJECT(DivineMote_Fly);
    BOF3_INJECT(DivineMote_DrawStar);
    BOF3_INJECT(DivineMote_DrawRing);
    BOF3_INJECT(DivineMote_Alloc);
}

// Moved here by tools/move_funcs.py from magic_c1.cpp (docs/source-layout-round.md).
#include "game/magic/pentagram.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c1.h"
#include "game/magic_harness.h"
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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Addr(const void* p) { return Low32(p); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only - what
// follows is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// Gfx_ClutStrip entries first..last - 1 from Gfx_ClutStripSource, with or
// without the STP bit: row 26's first CLUT (0x1A00..0x1A0F, MAGIC113 and 213)
// or entries 1..15 of its third (0x1A21..0x1A2F, MAGIC145 and 146, which then
// clear 0x1A20).
void ClutCopy(unsigned first, unsigned last, std::uint16_t stp) {
    for (unsigned k = first; k < last; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | stp);
}

// The GTE projections with the arguments the originals push (the depth and
// flag outputs in the caller's frame).
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define C1_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    C1_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}

void Rtp3(unsigned char* prim) {
    long p, flag;
    C1_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

void Rtp4(unsigned char* prim) {
    long p, flag;
    C1_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A child of MAGIC113's task: kind 1, parameter 3 (PentagramChild_Task), its
// owner this task, its phase +1 `kind` (0 a ring, 1 the figure, 2 the band, 3
// a sprite), its direction the owner's +8 - 2 & 3, +9 and +0xA 0, at the
// task's position and height. Sprite_Current and the owner are read after the
// create.
unsigned char* NewChild(unsigned kind) {
    const unsigned slot = NewTask(3);
    unsigned char* const self = Sc();
    records::BattleTask* const tself = rv::As<records::BattleTask>(self);
    const unsigned char* const owner = Owner();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Addr(self));
    child[1] = static_cast<unsigned char>(kind);
    child[8] = static_cast<unsigned char>((owner[8] - 2) & 3);
    child[9] = 0;
    child[0xA] = 0;
    tchild->x = tself->x;
    tchild->z = tself->z;
    rv::Store<unsigned>(tchild->height_whole, tself->height_whole);
    return child;
}

// The ring's and figure's line colour: (0xF0, 0xAA, 0xC8), dimmed by +0xA
// (x 20, x 14, x 16) while +2 is 2 (the fade), each byte read again.
void LineColour(unsigned char* p) {
    p[4] = 0xF0;
    p[5] = 0xAA;
    p[6] = 0xC8;
    if (Sc()[2] != 2) return;
    p[4] = static_cast<unsigned char>(0xF0 - Sc()[0xA] * 20);
    p[5] = static_cast<unsigned char>(0xAA - Sc()[0xA] * 14);
    p[6] = static_cast<unsigned char>(0xC8 - (Sc()[0xA] << 4));
}

}  // namespace

// original 0x4D67F0: the kind-2 task. An eight-entry stack table by +1 -
// Start, Rings, Star, Band, Sprites, WaitSprites, WaitChildren and MAGIC114's
// 0x4D7BD0 - then, while +0 is set, the disc under the actor's matrix.
C1_EXPORT void __cdecl Pentagram_Task(void) {
    static constexpr std::uint32_t kPhases[8] = {bof3::addr::Pentagram_Start,        bof3::addr::Pentagram_Rings,
                                                 bof3::addr::Pentagram_Star,         bof3::addr::Pentagram_Band,
                                                 bof3::addr::Pentagram_Sprites,      bof3::addr::Pentagram_WaitSprites,
                                                 bof3::addr::Pentagram_WaitChildren, bof3::addr::Magic114_End};
    StackCall(kPhases, 8, Sc()[1], "Pentagram_Task");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::Pentagram_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D6860: when the target and the actor are on the same side (both
// below 3 or both 3 and up) the target's flag 0x40, the done flag and the task
// freed - nothing drawn; else the task at the source sprite's position, row
// 26's first sixteen CLUT entries from their source with the STP bit,
// Gfx_ClutStripDirty, +0xB and +9 0, +1 on.
C1_EXPORT void __cdecl Pentagram_Start(void) {
    const unsigned char target = TargetByte();
    const unsigned char actor = Mem(::bof3::Img(at::kActor))[0];
    if ((target < 3) == (actor < 3)) {
        MH_CALL(Battle_SetTargetFlag40)(target);
        Mem(::bof3::Img(at::kFlags))[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    const unsigned char* const src = Source();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    ClutCopy(0x1A00, 0x1A10, 0x8000);
    Gfx_ClutStripDirty = 1;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4D6900: +9 up; at 0x10 two rings (children of kind 0, +0xB 0 and
// 1: their radius), sound 0x101, +1 on.
C1_EXPORT void __cdecl Pentagram_Rings(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    for (unsigned b = 0; b < 2; ++b) NewChild(0)[0xB] = static_cast<unsigned char>(b);
    MH_CALL(Sound_PlayEffect)(0x101);
    Inc(Sc()[1]);
}

// original 0x4D6A30: once the rings set +0xB to 3, the figure (a child of
// kind 1) and +1 on.
C1_EXPORT void __cdecl Pentagram_Star(void) {
    if (Sc()[0xB] != 3) return;
    NewChild(1);
    Inc(Sc()[1]);
}

// original 0x4D6AC0: once the figure sets +0xB to 4, the band (a child of
// kind 2), +1 on, sound 0x102, then the bytes 0x803154 0x7F and 0x92BF14 0.
C1_EXPORT void __cdecl Pentagram_Band(void) {
    if (Sc()[0xB] != 4) return;
    NewChild(2);
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x102);
    Mem(::bof3::Img(0x803154))[0] = 0x7F;
    Mem(::bof3::Img(0x92BF14))[0] = 0;
}

// original 0x4D6B70: two dropped calls (Port_DroppedCall, a bare ret, with
// 0x14, 0x10 and with 0x14) every frame; once the band sets +0xB to 5, six
// sprites (children of kind 3: +4 i & 1, +9 (i >> 1) * 8 + 1, +0xB i >> 1),
// +0xB 0, +1 on.
C1_EXPORT void __cdecl Pentagram_Sprites(void) {
    MH_AT(void (__cdecl*)(unsigned, unsigned), bof3::addr::Port_DroppedCall)(0x14, 0x10);
    MH_AT(void (__cdecl*)(unsigned), bof3::addr::Port_DroppedCall)(0x14);
    if (Sc()[0xB] != 5) return;
    unsigned char* self = nullptr;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(3);
        self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Addr(self));
        child[1] = 3;
        child[4] = static_cast<unsigned char>(i & 1);
        child[8] = static_cast<unsigned char>((Owner()[8] - 2) & 3);
        child[9] = static_cast<unsigned char>(((i >> 1) << 3) + 1);
        child[0xA] = 0;
        child[0xB] = static_cast<unsigned char>(i >> 1);
        tchild->x = rv::As<records::BattleTask>(self)->x;
        tchild->z = rv::As<records::BattleTask>(self)->z;
        rv::Store<unsigned>(tchild->height_whole, rv::As<records::BattleTask>(self)->height_whole);
    }
    // the task as Sprite_Current was read after the last create
    self[0xB] = 0;
    Inc(Sc()[1]);
}

// original 0x4D6C60: once the six sprites have counted +0xB up to 6, +0xB
// 0x84 (the children's release) and +1 on.
C1_EXPORT void __cdecl Pentagram_WaitSprites(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 6) return;
    s[0xB] = 0x84;
    Inc(Sc()[1]);
}

// original 0x4D6C80: once the four children have counted +0xB down to 0x80,
// +1 on.
C1_EXPORT void __cdecl Pentagram_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 0x80) Inc(s[1]);
}

// original 0x4D6C90: the disc - 32 gouraud triangles from the centre to a
// circle of radius 0x1C0, semi-transparent, the centre's shade +9 x 12 and
// the rim 1; a draw mode (tpage 0x55) before, one (0x15) after, all to slot 5.
C1_EXPORT void __cdecl Pentagram_DrawDisc(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSD(0, 0x1C0);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
    for (int angle = 0x80; angle < 0x1080; angle += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, z);
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const std::uint32_t shade = Sc()[9] * 12u;
        SetSD(4, shade);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D6E30: the child's kind-1 task, a jmp through
// PentagramChild_Kinds (four entries) by +1: ring, figure, band, sprite.
C1_EXPORT void __cdecl PentagramChild_Task(void) {
    CellCall(Addr(PentagramChild_Kinds), 4, Sc()[1], "PentagramChild_Task");
}

// original 0x4D6E50: a call through PentagramRing_Phases (three entries) by
// +2; while +0 is set, the ring under the actor's matrix, its radius
// PentagramRing_Radii[+0xB] (unbounded) in the scratch dword.
C1_EXPORT void __cdecl PentagramRing_Run(void) {
    CellCall(Addr(PentagramRing_Phases), 3, Sc()[2], "PentagramRing_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    SetSD(0, static_cast<std::uint32_t>(Long(Mem(Addr(PentagramRing_Radii) + 4u * Sc()[0xB]))));
    Call0(bof3::addr::PentagramRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D6EA0: +9 up; at 0x21 the owner's +0xB 3 and +2 on.
C1_EXPORT void __cdecl PentagramRing_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x21) return;
    Owner()[0xB] = 3;
    Inc(Sc()[2]);
}

// original 0x4D6ED0: the ring - up to 32 flat lines round a circle of the
// radius in the scratch dword, the first +9 - 1 of them (a line i drawn while
// +9 is above i), semi-transparent in the fade; a draw mode (0x35) before and
// one (0x15) after, slot 5. Each line starts where the last drawn one ended.
C1_EXPORT void __cdecl PentagramRing_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SD(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(4, 0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
    for (int i = 1, angle = 0x80; angle < 0x1080; ++i, angle += 0x80) {
        if (static_cast<int>(Sc()[9]) <= i) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        if (Sc()[2] == 2) MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0, static_cast<unsigned>(Mul12(v, SD(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
        SetVW(4, 0);
        Rtp1(0, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        LineColour(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D7080: a call through PentagramStar_Phases (three entries) by
// +2; while +0 is set, the figure under the actor's matrix.
C1_EXPORT void __cdecl PentagramStar_Run(void) {
    CellCall(Addr(PentagramStar_Phases), 3, Sc()[2], "PentagramStar_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::PentagramStar_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D70B0: +9 up; at 0x28 the owner's +0xB 4 and +2 on.
C1_EXPORT void __cdecl PentagramStar_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x28) return;
    Owner()[0xB] = 4;
    Inc(Sc()[2]);
}

// original 0x4D70E0 (the ring's and the figure's): once the owner's +0xB is
// 0x84, +2 on.
C1_EXPORT void __cdecl PentagramFx_WaitRelease(void) {
    if (Owner()[0xB] == 0x84) Inc(Sc()[2]);
}

// original 0x4D7100 (the ring's and the figure's): +0xA up; at 0xC the
// owner's +0xB down and the task freed.
C1_EXPORT void __cdecl PentagramFx_Fade(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0xC) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D7130: the figure - five flat lines between the six points of
// radius 0x120 at the angles PentagramStar_Points[i] x 0x111, each drawn
// (+9 - 8 i) / 8 of its length (up to all of it) once +9 passes 8 i; the
// ring's colours; draw modes 0x35 / 0x15, slot 5.
C1_EXPORT void __cdecl PentagramStar_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const auto* const points = reinterpret_cast<const signed char*>(Mem(Addr(PentagramStar_Points)));
    for (int i = 0; i < 5; ++i) {
        const int n = Sc()[9];
        if (8 * i >= n) continue;
        int len = n - 8 * i;
        if (len >= 8) len = 8;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        if (Sc()[2] == 2) MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(points[i] * 0x111);
        SetVW(0, static_cast<unsigned>(Mul12(v, 288)));
        v = MH_CALL(Math_Cos)(points[i] * 0x111);
        SetVW(2, static_cast<unsigned>(Mul12(v, 288)));
        SetVW(4, 0);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        v = MH_CALL(Math_Sin)(points[i + 1] * 0x111);
        SetVW(8, static_cast<unsigned>(Mul12(v, 288)));
        v = MH_CALL(Math_Cos)(points[i + 1] * 0x111);
        const int c = Mul12(v, 288);
        SetVW(8, VW(0) + static_cast<unsigned>((VS(8) - VS(0)) / 8 * len));
        SetVW(0xA, VW(2) + static_cast<unsigned>((static_cast<short>(c) - VS(2)) / 8 * len));
        SetVW(0xC, 0);
        Rtp1(8, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        LineColour(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D7380: a call through PentagramBand_Phases (three entries) by
// +2; while +0 is set, the band under the actor's matrix.
C1_EXPORT void __cdecl PentagramBand_Run(void) {
    CellCall(Addr(PentagramBand_Phases), 3, Sc()[2], "PentagramBand_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::PentagramBand_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D73B0: +9 up; at 0x31 the owner's +0xB 5 and +2 on.
C1_EXPORT void __cdecl PentagramBand_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x31) return;
    Owner()[0xB] = 5;
    Inc(Sc()[2]);
}

// original 0x4D73E0: once the owner's +0xB is 0x84, sound 0x103 and +2 on.
C1_EXPORT void __cdecl PentagramBand_WaitRelease(void) {
    if (Owner()[0xB] != 0x84) return;
    MH_CALL(Sound_PlayEffect)(0x103);
    Inc(Sc()[2]);
}

// original 0x4D7410: +0xA up; at 0x10 the owner's +0xB down and the task freed.
C1_EXPORT void __cdecl PentagramBand_Fade(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D7440: the band - up to 32 gouraud quads round a circle of
// radius 0x130, standing from the ground (height +0xA x 8) to the height in
// the scratch dword +8 below it: (+9 - i) x 32 for the last sixteen drawn,
// else 0x200 + Rand & 0x3F (0x200 exactly at i 32); a quad i drawn while +9
// is above i, each sorted at its far rim point; semi-transparent, black at the
// foot, (0xC0 - +0xA x 12, (0x10 - +0xA) x 2, same) at the top. Draw mode
// 0x35 before each, 0x15 after them all (slot 3).
C1_EXPORT void __cdecl PentagramBand_Draw(void) {
    {
        const unsigned b = Sc()[9];
        SetSD(8, b < 0x10 ? b << 5 : 0x200);
    }
    int v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, 304)));
    v = MH_CALL(Math_Sin)(0);
    {
        const std::uint32_t top = static_cast<std::uint32_t>(SD(8));
        SetVW(0xA, static_cast<unsigned>(Mul12(v, 304)));
        SetVW(0xC, (Sc()[0xA] << 3) - top);
    }
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, 304)));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, 304)));
    for (int i = 1; i < 0x21; ++i) {
        const int n = Sc()[9];
        if (i >= n) continue;
        const int d = n - i;
        if (d < 0x10) {
            SetSD(8, static_cast<std::uint32_t>(d) << 5);
        } else {
            const std::uint32_t r = RandCall();
            SetSD(8, (r & 0x3F) + 0x200);
            if (i == 0x20) SetSD(8, 0x200);
        }
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        SetVW(4, VW(0xC));
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        SetVW(0x14, 0);
        const int angle = i << 7;
        v = MH_CALL(Math_Cos)(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, 304)));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, 304)));
        SetVW(0xC, (Sc()[0xA] << 3) - static_cast<std::uint32_t>(SD(8)));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, 304)));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0x1C, 0);
        const short zr = static_cast<short>(Mul12(v, 304));
        SetVW(0x1A, static_cast<unsigned short>(zr));
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(VS(0x18))) << 9) + static_cast<std::uint32_t>(ts->x);
        const std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(zr)) << 9) + static_cast<std::uint32_t>(ts->z);
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSD(4, (0x10u - Sc()[0xA]) << 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = static_cast<unsigned char>(0xC0 - Sc()[0xA] * 12);
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        p[0x34] = static_cast<unsigned char>(0xC0 - Sc()[0xA] * 12);
        p[0x35] = SB(4);
        p[0x36] = SB(4);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D7760: a draw mode (0xB5, slot 3); with the frame-offset table
// the effects' (0x8E3580), a call through PentagramSprite_Phases (three
// entries: MAGIC130's count-down 0x4E47F0, Start, Animate) by +2; the table
// back (0x8B3580); a draw mode (0x95, slot 3).
C1_EXPORT void __cdecl PentagramSprite_Run(void) {
    DrawMode(0xB5);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    CellCall(Addr(PentagramSprite_Phases), 3, Sc()[2], "PentagramSprite_Run");
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
    DrawMode(0x95);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D77D0: the sprite raised 0x180 (+0xB 2) and 0x200 more (+4 set)
// in +0x3C's high word; its sprite fields set up (+0x25 0x1D, +0x26 0, +0x27
// 0xA0, +0x28 0, +0 bit 0x20, colour (0xC0, 0xC0, 0xC0) with +0x5C 1, +0x2A
// its +4, +0x29 and +0x24 4, +0x2C 0, +0x2B 1), +9 0, +2 on; sound 0x100 for
// +0xB 0; animation +0xB.
C1_EXPORT void __cdecl PentagramSprite_Start(void) {
    unsigned char* s = Sc();
    if (s[0xB] == 2) {
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->height) + 0x1800000u));
        s = Sc();
    }
    if (s[4] != 0) {
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->height) + 0x2000000u));
        s = Sc();
    }
    s[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0] |= 0x20;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Sc()[0x5C] = 1;
    Sc()[0x2A] = Sc()[4];
    Sc()[0x29] = 4;
    Sc()[0x24] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 1;
    Sc()[9] = 0;
    Inc(Sc()[2]);
    if (Sc()[0xB] == 0) MH_CALL(Sound_PlayEffect)(0x100);
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
}

// original 0x4D78D0: the sprite's script ticked; when it reports an end, +9
// up - past 2 the owner's +0xB up and the task freed; else +0x2A (+0x2A - 1)
// & 1, the animation +0xB again, the screen update, and for +0xB 0 sound
// 0x104 (+9 1) or 0x100. While the script runs, the screen update only.
C1_EXPORT void __cdecl PentagramSprite_Animate(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
        MH_CALL(Sprite_UpdateScreen)();
        return;
    }
    Inc(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] > 2) {
        Inc(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    s[0x2A] = static_cast<unsigned char>((s[0x2A] - 1) & 1);
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
    MH_CALL(Sprite_UpdateScreen)();
    s = Sc();
    if (s[0xB] != 0) return;
    MH_CALL(Sound_PlayEffect)(s[9] == 1 ? 0x104 : 0x100);
}

void Pentagram_Inject() {
    BOF3_INJECT(Pentagram_Task);
    BOF3_INJECT(Pentagram_Start);
    BOF3_INJECT(Pentagram_Rings);
    BOF3_INJECT(Pentagram_Star);
    BOF3_INJECT(Pentagram_Band);
    BOF3_INJECT(Pentagram_Sprites);
    BOF3_INJECT(Pentagram_WaitSprites);
    BOF3_INJECT(Pentagram_WaitChildren);
    BOF3_INJECT(Pentagram_DrawDisc);
    BOF3_INJECT(PentagramChild_Task);
    BOF3_INJECT(PentagramRing_Run);
    BOF3_INJECT(PentagramRing_Grow);
    BOF3_INJECT(PentagramRing_Draw);
    BOF3_INJECT(PentagramStar_Run);
    BOF3_INJECT(PentagramStar_Grow);
    BOF3_INJECT(PentagramFx_WaitRelease);
    BOF3_INJECT(PentagramFx_Fade);
    BOF3_INJECT(PentagramStar_Draw);
    BOF3_INJECT(PentagramBand_Run);
    BOF3_INJECT(PentagramBand_Grow);
    BOF3_INJECT(PentagramBand_WaitRelease);
    BOF3_INJECT(PentagramBand_Fade);
    BOF3_INJECT(PentagramBand_Draw);
    BOF3_INJECT(PentagramSprite_Run);
    BOF3_INJECT(PentagramSprite_Start);
    BOF3_INJECT(PentagramSprite_Animate);
}

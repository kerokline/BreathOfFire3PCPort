// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/magic/geo_breath_child.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

}  // namespace

namespace {

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS37 = move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

// kGeoPool: now GeoBreath_Pool (symbols.toml). GeoBreath_Pool, 64 records
constexpr unsigned kGeoRecords = 64;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(LongMagicS37(at)) + v)); }

void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

// `movsx` of a byte, then `shl 2`, as the word the originals store.
unsigned SignedShl2(unsigned char b) { return static_cast<unsigned>(static_cast<int>(static_cast<signed char>(b))) << 2; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* GeoRecord(unsigned index) { return Mem(::bof3::Img(GeoBreath_Pool_addr) + index * 0x84u); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A kind-2 task's walk of its pool (after its phase): every record with bit 0
// of +0 is run through `task` as Sprite_Current, its +0x80 the owner; the
// task's own two cells, read once after the phase, put back after each.
void WalkPool(std::uint32_t pool, unsigned records, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = LongMagicS37(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = LongMagicS37(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The first of a pool's records without bit 0 of +0 gets it; its index, 0xFF
// when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// A pool's bytes +0..+2 of every record cleared.
void ClearPool(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// Gfx_ClutStrip's row 26 (0x1A00..0x1AFF) back from its source with the
// semi-transparency bit, then the first `row2` words of row 2 (0x200..) the
// same; the first word of each plain (26's first); the strip dirty.
void RestoreRow26ThenRow2(unsigned row2) {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x200 + row2; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    const unsigned short first26 = Gfx_ClutStripSource[0x1A00], first2 = Gfx_ClutStripSource[0x200];
    Gfx_ClutStrip[0x1A00] = first26;
    Gfx_ClutStrip[0x200] = first2;
    Gfx_ClutStripDirty = 1;
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S37_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// All four projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// Gfx_PacketNext's primitive of `size` linked at Sprite_Current's point.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// The draws' shared shape: a fan of 64 semi-transparent gouraud triangles
// round the origin (Prim_VertexScratch: the origin, the last rim point, the
// next at the angle; the radius the word 0x903850, all nine colour bytes the
// byte 0x90385A, both read again after every call), tpage 0x55, layer 5.
// The caller has set the two words.
void DrawGlowFan() {
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// The other: a ring of 64 semi-transparent gouraud quads between the radius
// word 0x903852 (vertices 0 and 1, colour 1) and the radius word 0x903850
// (vertices 2 and 3, the byte 0x90385A), tpage 0x55, layer 5.
void DrawRingBand() {
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int inner = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(inner));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = Sin(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
}

// The textured quad of a record or mote: page (0x340, 0x100) abr 1, clut
// (0, 0x1E2), the texture's 0x20 x 0x20 at (0, 0x80); corners at the angles
// 0x200, 0x600, 0xE00, 0xA00, radius 0x100; the colour the byte 0x90385A
// (the word +0xA x `shade`, +0xA read after the packet is set up); tpage
// 0x35, linked at the sprite's point before and after.
void DrawTexturedQuad(unsigned shade) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const unsigned b = Sc()[0xA];
        SetSW(0, 0x100);
        SetSW(0xA, b * shade);
    }
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        SetVW(c * 8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(kAngles[c]);
        SetVW(c * 8 + 2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(c * 8 + 4, 0);
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x14] = 0;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = SB(0xA);
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    Rtp4(p);
    MH_CALL(Gte_PrimDepths4_10)(p);
    LinkAtSprite(0x48);
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// Sixteen GeoBreath_Pool records (a full pool skips one): +0x80 the owner
// (the kind-2 task), +1 0, +0xB the number; the owner's count +0xB up.
void SpawnGeoRecords() {
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned index = MH_AT(ByteFn, addr::GeoBreath_PoolAlloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = GeoRecord(index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        unsigned char* const owner = Owner();
        trec->owner = static_cast<std::int32_t>(Key(owner));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        Inc(owner[0xB]);
    }
}

// The child shaken about its owner's point by +0xC: (+, -) on odd frames
// (Frame_Counter's bit 0), (-, +) on even ones.
void ShakeAboutOwner() {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    const std::uint32_t d = U(ts->step_x);
    if ((Frame_Counter & 1) != 0) {
        ts->x = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) + d);
        ts->z = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) - d);
    } else {
        ts->x = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) - d);
        ts->z = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) + d);
    }
}

// Every eighth frame (Frame_Counter & 7 0): sound 0x102 and sixteen records.
void BurstOnEighthFrame() {
    if ((Frame_Counter & 7) != 0) return;
    Sound(0x102);
    SpawnGeoRecords();
}

// The record moved out along the angle +0xB << 8 (the word 0x903854) by
// sin / cos x `times`: each cell's address taken before its call, as the
// original holds it in esi.
void MoveAlongAngle(int times) {
    {
        unsigned char* const s = Sc();
        const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(s[0xB]) << 8);
        SetSW(4, angle);
        unsigned char* const cell = s + 0x34;
        const int v = Sin(static_cast<short>(angle));
        AddLong(cell, Mul(v, times));
    }
    {
        const short angle = SS(4);
        unsigned char* const cell = Sc() + 0x38;
        const int v = Cos(angle);
        AddLong(cell, Mul(v, times));
    }
}

}  // namespace

// original 0x4F62C0: the kind-2 task. A three-entry stack table by +1
// (GeoBreath_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish), unchecked;
// then the pool walked through GeoBreathRecord_Task.
S37_EXPORT void __cdecl GeoBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::GeoBreath_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "GeoBreath_Task");
    WalkPool(::bof3::Img(GeoBreath_Pool_addr), kGeoRecords, addr::GeoBreathRecord_Task);
}

// original 0x4F6340: the pool's +0..+2 cleared; the task at the side's
// centre, then at the field's kind-2 point (Field_Kind2X / Z); +0xB 0, +9
// 0x38, +1 on; sound 0x100; one child (kind 1, 0x61), +0x80 this task, +1 0,
// counted in +0xB; CLUT row 26 back whole and the first 16 words of row 2
// with their STP bits, the first word of each without.
S37_EXPORT void __cdecl GeoBreath_Start(void) {
    ClearPool(::bof3::Img(GeoBreath_Pool_addr), kGeoRecords);
    MH_CALL(MagicFx_CenterOnSide)();
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = Field_Kind2X;
        ts->z = Field_Kind2Z;
        s[0xB] = 0;
        s[9] = 0x38;
        Inc(s[1]);
    }
    Sound(0x100);
    {
        const unsigned slot = NewTask(0x61);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        Inc(self[0xB]);
    }
    RestoreRow26ThenRow2(0x10);
}

// original 0x4F6440: the child's kind-1 task, a jmp through
// GeoBreathChild_TaskTable (one entry) by +1, unchecked.
S37_EXPORT void __cdecl GeoBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::GeoBreathChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "GeoBreathChild_Task");
}

// original 0x4F6460: with the frame-offset table the effects', a call through
// GeoBreathChild_Steps (eight: _Start, _Swirl, _Burst, _Shake, _Quake,
// _Settle, group S18's BuffRing_Wait, _End) by +2; then while bit 0 of +0 is
// set and +2 is not 0: the sprite on the screen, the record matrix, the glow,
// the ring, the matrix popped; the battle's table back.
S37_EXPORT void __cdecl GeoBreathChild_Run(void) {
    static constexpr std::uint32_t kSteps[8] = {
        addr::GeoBreathChild_Start, addr::GeoBreathChild_Swirl,  addr::GeoBreathChild_Burst, addr::GeoBreathChild_Shake,
        addr::GeoBreathChild_Quake, addr::GeoBreathChild_Settle, addr::BuffRing_Wait,         addr::GeoBreathChild_End};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 8, Sc()[2], "GeoBreathChild_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::GeoBreathChild_DrawGlow);
        Call0(addr::GeoBreathChild_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4F64C0: the child at its owner's point, 0x1600000 above it; its
// sprite fields, animation 0; +0x5D 0x10, +0xB 0, +9 0xA, +0xA 0, +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Start(void) {
    {
        unsigned char* const s = Sc();
        CopyLong(s + 0x34, Owner() + 0x34);
        CopyLong(s + 0x38, Owner() + 0x38);
        rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->height) + 0x1600000u));
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        rv::Store(rv::As<records::BattleTask>(s)->scale_x, 0x10000);
        rv::Store(rv::As<records::BattleTask>(s)->scale_y, 0x10000);
        s[0x48] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
        s[0x2B] = 0;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    unsigned char* const s = Sc();
    s[0x5D] = 0x10;
    s[0xB] = 0;
    s[9] = 0xA;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4F65D0: the script ticked once (its answer unread); the glow's
// radius +0xB up by 0xC; +9 down, at 0 +9 0x1E, +0xA 3 (bursts) and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Swirl(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 0xC);
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 0x1E;
    s[0xA] = 3;
    Inc(s[2]);
}

// original 0x4F6620: +9 down; at 0 +0xA (bursts left) down: at 0
// MapView_Redraw 0x5D, +9 0x3C and +2 4 (the quake); else sixteen records,
// sound 0x101 at the second-last burst, sound 0x102, the shake +0xC 0x800,
// +9 4, +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Burst(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        Dec(s[0xA]);
        if (s[0xA] == 0) {
            MapView_Redraw = 0x5D;
            s[9] = 0x3C;
            Sc()[2] = 4;
            return;
        }
    }
    SpawnGeoRecords();
    if (Sc()[0xA] == 2) Sound(0x101);
    Sound(0x102);
    rv::Current<records::BattleTask>()->step_x = 0x800;
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4F6700: shaken about the owner; +9 down, at 0 +9 8 and +2 back
// one (the next burst).
S37_EXPORT void __cdecl GeoBreathChild_Shake(void) {
    ShakeAboutOwner();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 8;
    Dec(Sc()[2]);
}

// original 0x4F6780: shaken; every eighth frame a burst; Camera_ShiftX 0x10
// on odd +9, 0 on even; +9 down, at 0 +9 0x20 and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Quake(void) {
    ShakeAboutOwner();
    BurstOnEighthFrame();
    unsigned char* const s = Sc();
    Camera_ShiftX = static_cast<short>((s[9] & 1u) << 4);
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4F6880: shaken; every eighth frame a burst; Camera_ShiftX +9 >>
// 1 on odd +9, 0 on even; +9 down, at 0 Camera_ShiftX 0, +9 8 and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Settle(void) {
    ShakeAboutOwner();
    BurstOnEighthFrame();
    unsigned char* const s = Sc();
    const unsigned char c = s[9];
    Camera_ShiftX = static_cast<short>((c & 1) != 0 ? c >> 1 : 0);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Camera_ShiftX = 0;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F6990: the glow's shade +0x5D down by 2 unless 0; the script
// ticked once, and at its end the owner's count +0xB down and the task freed
// (a tail jmp).
S37_EXPORT void __cdecl GeoBreathChild_End(void) {
    {
        unsigned char* const s = Sc();
        if (s[0x5D] != 0) s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
    }
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFFu) == 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F69C0: a glow of 64 semi-transparent gouraud triangles round
// the origin, radius +0xB, every colour byte the signed +0x5D x 4; tpage 0x55,
// layer 5 (the fan of MAGIC053's LavaburstChild_DrawGlow with its own words).
S37_EXPORT void __cdecl GeoBreathChild_DrawGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(0xA, SignedShl2(s[0x5D]));
    }
    DrawGlowFan();
}

// original 0x4F6B60: a ring of 64 semi-transparent gouraud quads from radius
// +0xB x 2 (dark) in to +0xB (the signed +0x5D x 4); tpage 0x55, layer 5.
S37_EXPORT void __cdecl GeoBreathChild_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, SignedShl2(s[0x5D]));
    }
    DrawRingBand();
}

// original 0x4F6D90: a record's task, a jmp through GeoBreathRecord_TaskTable
// (one entry) by +1, unchecked.
S37_EXPORT void __cdecl GeoBreathRecord_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::GeoBreathRecord_Run};
    Dispatch(kKinds, 1, Sc()[1], "GeoBreathRecord_Task");
}

// original 0x4F6DB0: a call through GeoBreathRecord_Steps (three) by +2; then
// while +0 and +2 are not 0 the actor matrix, the quad, the matrix popped (a
// tail jmp).
S37_EXPORT void __cdecl GeoBreathRecord_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::GeoBreathRecord_Start, addr::GeoBreathRecord_Fly,
                                                addr::GeoBreathRecord_Fade};
    Dispatch(kSteps, 3, Sc()[2], "GeoBreathRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::GeoBreathRecord_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F6DF0: the record at the angle +0xB << 8 (the word 0x903854)
// round its owner, radius 24 (sin / cos x 24, unshifted), at the owner's
// height; +9 8, +0xA 0x10, +2 on.
S37_EXPORT void __cdecl GeoBreathRecord_Start(void) {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 8);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) + Mul(v, 24)));
    v = Cos(SS(4));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) + Mul(v, 24)));
    CopyLong(Sc() + 0x3C, Owner() + 0x3C);
    unsigned char* const s = Sc();
    s[9] = 8;
    s[0xA] = 0x10;
    Inc(s[2]);
}

// original 0x4F6E80: out by sin / cos x 3; +9 down, at 0 +2 on.
S37_EXPORT void __cdecl GeoBreathRecord_Fly(void) {
    MoveAlongAngle(3);
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4F6EF0: out by sin / cos x 2; the shade +0xA down by 2, at 0 the
// owner's count +0xB down and the record freed (MagicFx_FreeCurrentRecord, a
// tail jmp).
S37_EXPORT void __cdecl GeoBreathRecord_Fade(void) {
    MoveAlongAngle(2);
    unsigned char* const s = Sc();
    s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F6F70: the textured quad, its colour +0xA x 6.
S37_EXPORT void __cdecl GeoBreathRecord_Draw(void) { DrawTexturedQuad(6); }

// original 0x4F7190: the first of GeoBreath_Pool's 64 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S37_EXPORT unsigned char __cdecl GeoBreath_PoolAlloc(void) { return PoolAlloc(::bof3::Img(GeoBreath_Pool_addr), kGeoRecords); }

void GeoBreathChild_Inject() {
    BOF3_INJECT(GeoBreath_Task);
    BOF3_INJECT(GeoBreath_Start);
    BOF3_INJECT(GeoBreathChild_Task);
    BOF3_INJECT(GeoBreathChild_Run);
    BOF3_INJECT(GeoBreathChild_Start);
    BOF3_INJECT(GeoBreathChild_Swirl);
    BOF3_INJECT(GeoBreathChild_Burst);
    BOF3_INJECT(GeoBreathChild_Shake);
    BOF3_INJECT(GeoBreathChild_Quake);
    BOF3_INJECT(GeoBreathChild_Settle);
    BOF3_INJECT(GeoBreathChild_End);
    BOF3_INJECT(GeoBreathChild_DrawGlow);
    BOF3_INJECT(GeoBreathChild_DrawRing);
    BOF3_INJECT(GeoBreathRecord_Task);
    BOF3_INJECT(GeoBreathRecord_Run);
    BOF3_INJECT(GeoBreathRecord_Start);
    BOF3_INJECT(GeoBreathRecord_Fly);
    BOF3_INJECT(GeoBreathRecord_Fade);
    BOF3_INJECT(GeoBreathRecord_Draw);
    BOF3_INJECT(GeoBreath_PoolAlloc);
}

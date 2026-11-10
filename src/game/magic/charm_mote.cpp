// Moved here by tools/move_funcs.py from magic_s34.cpp (docs/source-layout-round.md).
#include "game/magic/charm_mote.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS34 = move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The three pools. MAGIC158's and MAGIC166's hold 64 task-like records of
// 0x84 bytes (+0x80 the owner); MAGIC162's 128 records of 0x2C bytes (+0x28
// the owner), run through a "current record" pointer that follows the pool.
// kCharmPool: now Charm_Pool (symbols.toml).
// kTransferPool: now Transfer_Pool (symbols.toml).
// kTransferCurrent: now TransferMote_Current (symbols.toml).
// kMonopolizePool: now Monopolize_Pool (symbols.toml).
constexpr unsigned kMotePools = 64;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(LongMagicS34(at)) + v));
}

void AddWord(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code), in the fuzz that
// address's recorder.
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

// The callees with the arguments the originals push: Gte_RotTransPers4 gets
// the depth and flag pointers, Gte_RotTrans a flag pointer; ours reads
// neither flag.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

using RotTransFn = void (__cdecl*)(const short*, long*, long*);

#define S34_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four SVECTORs projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S34_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// Linked at the sprite's (+0x34, +0x38), layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// A draw-mode packet (tpage 0x35, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

unsigned char* MoteRecord(std::uint32_t pool, unsigned index) { return Mem(pool + index * 0x84u); }

// A pool walked: every record with bit 0 of +0 becomes Sprite_Current, its
// +0x80 the owner, for its task; both put back after each. The task and owner
// are read once, after the phase call that precedes the walk.
void WalkMotes(std::uint32_t pool, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = LongMagicS34(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kMotePools; ++i) {
        unsigned char* const rec = MoteRecord(pool, i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The pool's +0..+2 cleared; the task at the source sprite (its facing and
// point), +0xB 0, +1 on; `motes` records from the pool's allocator (a full
// pool skips one), each owned by this task, +0 or 0x41, +0xB its number, +9
// a delay of 1..0x20, counted in +0xB; sound 0x100.
void StartMotes(std::uint32_t pool, unsigned motes, std::uint32_t alloc) {
    for (unsigned i = 0; i < kMotePools; ++i) {
        unsigned char* const rec = MoteRecord(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        s[8] = src[8];
        ts->x = rv::As<records::SpriteHead>(src)->x;
        ts->z = rv::As<records::SpriteHead>(src)->z;
        rv::Store(ts->height, rv::As<records::SpriteHead>(src)->height);
        s[0xB] = 0;
        Inc(s[1]);
    }
    for (unsigned i = 0; i < motes; ++i) {
        const unsigned index = MH_WORD(ByteFn, alloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = MoteRecord(pool, index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Key(Sc()));
        rec[0] = static_cast<unsigned char>(rec[0] | 0x41);
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 0x1F) + 1);
        Inc(Sc()[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// The first of `count` records of `stride` bytes without bit 0 of +0 gets it;
// its index in al, 0xFF when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned count, unsigned stride) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const rec = Mem(pool + i * stride);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// A mote's run: its step by +2; while +0 and +2 are set, MAGIC158's matrix,
// its fan, the matrix popped.
void RunMote(const std::uint32_t* steps, unsigned n, const char* who) {
    Dispatch(steps, n, Sc()[2], who);
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::CharmMote_PushMatrix);
    Call0(addr::CharmMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// A mote's launch: +9 down; at 0 the mote at the angle (+0xB & 0xF) << 8
// round the owner at radius 8 (the sine and cosine << 15 >> 12), `lift` above
// it; a colour of eight (Rand & 7) into +0x5D..+0x5F; +0x14 0x80, +0x20
// `fall`, +9 / +0xA as given, +2 on.
void LaunchMote(std::uint32_t colours, std::uint32_t lift, std::uint32_t fall, unsigned char count, unsigned char size) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned angle = (Sc()[0xB] & 0xFu) << 8;
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        const std::int32_t x = rv::As<records::SpriteHead>(Owner())->x;
        rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(v, 15, 12)) + static_cast<std::uint32_t>(x)));
    }
    v = Cos(SS(4));
    {
        const std::int32_t z = rv::As<records::SpriteHead>(Owner())->z;
        rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(v, 15, 12)) + static_cast<std::uint32_t>(z)));
    }
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + lift));
    const unsigned k = RandCall() & 7u;
    unsigned char* const s = Sc();
    const unsigned char* const c = Mem(colours + 3 * k);
    s[0x5D] = c[0];
    s[0x5E] = c[1];
    s[0x5F] = c[2];
    rv::Store(rv::As<records::BattleTask>(s)->step_y, 0x80);
    rv::Store(rv::As<records::BattleTask>(s)->work_20, static_cast<std::int32_t>(fall));
    s[9] = count;
    s[0xA] = size;
    Inc(s[2]);
}

// A mote's spiral: the point on by the sine and cosine of the angle (sign
// extended from 20 bits), each added through the pointer taken before its
// call; the fall +0x20 into the speed +0x14, the speed into the height +0x3E.
void SpiralMote(unsigned angle) {
    SetSW(4, angle);
    unsigned char* at = Sc() + 0x34;
    int v = Sin(static_cast<short>(angle));
    AddLong(at, static_cast<std::uint32_t>(ShlSar(v, 12, 12)));
    at = Sc() + 0x38;
    v = Cos(SS(4));
    AddLong(at, static_cast<std::uint32_t>(ShlSar(v, 12, 12)));
    unsigned char* const s = Sc();
    AddLong(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    AddWord(s + 0x3E, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
}

// The mote freed: the owner's count +0xB down, +0..+4 cleared.
void FreeMote(unsigned char* s) {
    Dec(Owner()[0xB]);
    s[0] = 0;
    s[1] = 0;
    s[2] = 0;
    s[3] = 0;
    s[4] = 0;
}

struct Matrix {
    short m[10];
    long t[3];
};

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4ED670: the kind-2 task. A two-entry stack table by +1
// (Charm_Start, BattleFx_Finish), then Charm_Pool walked through
// CharmMote_Task.
S34_EXPORT void __cdecl Charm_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Charm_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Charm_Task");
    WalkMotes(::bof3::Img(Charm_Pool_addr), addr::CharmMote_Task);
}

// original 0x4ED6F0: the pool cleared, the task at the source sprite, sixteen
// motes (StartMotes), sound 0x100.
S34_EXPORT void __cdecl Charm_Start(void) { StartMotes(::bof3::Img(Charm_Pool_addr), 16, addr::Charm_PoolAlloc); }

// original 0x4ED7D0: a mote's task, a jmp through CharmMote_TaskTable (one
// entry) by +1, unchecked.
S34_EXPORT void __cdecl CharmMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::CharmMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "CharmMote_Task");
}

// original 0x4ED7F0: a call through CharmMote_Steps (three) by +2; while +0
// and +2 are set the matrix, the fan, the matrix popped.
S34_EXPORT void __cdecl CharmMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CharmMote_Launch, addr::CharmMote_ToOwner, addr::CharmMote_Fall};
    RunMote(kSteps, 3, "CharmMote_Run");
}

// original 0x4ED830: +9 down; at 0 round the owner, 0x480 units above it, a
// colour from Charm_Colours; +0x14 0x80, +0x20 -16, +9 8, +0xA 2, +2 on.
S34_EXPORT void __cdecl CharmMote_Launch(void) { LaunchMote(::bof3::Img(Charm_Colours_addr), 0x4800000, 0xFFFFFFF0u, 8, 2); }

// original 0x4ED940: the height word +0x3E down by 0x60; +9 up to 0x10, +0xA
// up by 2 to 0x10; +2 on once the height is the owner's.
S34_EXPORT void __cdecl CharmMote_ToOwner(void) {
    unsigned char* const s = Sc();
    AddWord(s + 0x3E, 0xFFA0);
    if (s[9] != 0x10) Inc(s[9]);
    if (s[0xA] != 0x10) s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (rv::As<records::BattleTask>(s)->height_whole == rv::As<records::SpriteHead>(Owner())->height_whole) Inc(s[2]);
}

// original 0x4ED990: the spiral at (+0xB & 0xF) << 8; +0xA down, at 0 the
// mote freed.
S34_EXPORT void __cdecl CharmMote_Fall(void) {
    SpiralMote((Sc()[0xB] & 0xFu) << 8);
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    FreeMote(s);
}

// original 0x4EDA60: the mote's matrix pushed: turned (0x400, 0xE00, the
// frame counter's low five bits << 7, inverted when bit 0 of +0xB is set),
// at (+0x34 >> 9) - 0x4000, (+0x38 >> 9) - 0x4000, -(s16 +0x3E / 2), after
// the camera's.
S34_EXPORT void __cdecl CharmMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    short rot[4];
    rot[0] = 0x400;
    rot[1] = 0xE00;
    const auto frame = static_cast<unsigned char>(Frame_Counter);
    if (s[0xB] & 1) rot[2] = static_cast<short>(((~static_cast<unsigned>(frame)) & 0x1Fu) << 7);
    else rot[2] = static_cast<short>((frame & 0x1Fu) << 7);
    rot[3] = 0;
    short v[4];
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    S34_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4EDB30: a draw-mode packet (tpage 0x35); then a fan of five
// semi-transparent flat quads (Gpu_SetPolyF4) from the centre, each between
// the angles 0x199 i, 0x199 (i + 1) and 0x199 (i + 2) (0 for the last) at
// radii +9 x 2 and +9 x 4, its colour +0x5D..+0x5F (signed) x +0xA; linked
// at the mote, layer 2.
S34_EXPORT void __cdecl CharmMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[9] * 2u);
        SetSW(2, s[9] * 4u);
        SetSW(6, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[0xA])));
        SetSW(8, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[0xA])));
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[0xA])));
    }
    for (int i = 0; i < 10; i += 2) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyF4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int a = (i + 1) * 0x199;
        int v = Sin(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
        SetVW(4, 0);
        a = i * 0x199;
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0xC, 0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
        const unsigned last = i == 8 ? 0u : static_cast<unsigned>((i + 2) * 0x199);
        SetSW(4, last);
        v = Sin(static_cast<short>(last));
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(SS(4));
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(0x14, 0);
        SetVW(0x18, 0);
        SetVW(0x1A, 0);
        SetVW(0x1C, 0);
        Rtp4(p, 0xC);
        MH_CALL(Gte_PrimDepths4_0C)(p);
        p[4] = SB(6);
        p[5] = SB(8);
        p[6] = SB(0xA);
        LinkAtSprite(0x38);
    }
}

// original 0x4EDD70: the first of Charm_Pool's 64 records without bit 0 of
// +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Charm_PoolAlloc(void) { return PoolAlloc(::bof3::Img(Charm_Pool_addr), kMotePools, 0x84); }

void CharmMote_Inject() {
    BOF3_INJECT(Charm_Task);
    BOF3_INJECT(Charm_Start);
    BOF3_INJECT(CharmMote_Task);
    BOF3_INJECT(CharmMote_Run);
    BOF3_INJECT(CharmMote_Launch);
    BOF3_INJECT(CharmMote_ToOwner);
    BOF3_INJECT(CharmMote_Fall);
    BOF3_INJECT(CharmMote_PushMatrix);
    BOF3_INJECT(CharmMote_Draw);
    BOF3_INJECT(Charm_PoolAlloc);
}

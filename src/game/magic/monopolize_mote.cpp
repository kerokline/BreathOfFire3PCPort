// Moved here by tools/move_funcs.py from magic_s34.cpp (docs/source-layout-round.md).
#include "game/magic/monopolize_mote.h"

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

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(LongMagicS34(at)) + v));
}

void AddWord(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

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

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EF210: the kind-2 task. A two-entry stack table by +1
// (Monopolize_Start, BattleFx_Finish), then Monopolize_Pool walked through
// MonopolizeMote_Task.
S34_EXPORT void __cdecl Monopolize_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Monopolize_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Monopolize_Task");
    WalkMotes(::bof3::Img(Monopolize_Pool_addr), addr::MonopolizeMote_Task);
}

// original 0x4EF290: the pool cleared, the task at the source sprite, 32
// motes (StartMotes), sound 0x100.
S34_EXPORT void __cdecl Monopolize_Start(void) { StartMotes(::bof3::Img(Monopolize_Pool_addr), 32, addr::Monopolize_PoolAlloc); }

// original 0x4EF370: a mote's task, a jmp through MonopolizeMote_TaskTable
// (one entry) by +1, unchecked.
S34_EXPORT void __cdecl MonopolizeMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::MonopolizeMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "MonopolizeMote_Task");
}

// original 0x4EF390: a call through MonopolizeMote_Steps (two) by +2; while
// +0 and +2 are set MAGIC158's matrix and fan, the matrix popped.
S34_EXPORT void __cdecl MonopolizeMote_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {addr::MonopolizeMote_Launch, addr::MonopolizeMote_Fall};
    RunMote(kSteps, 2, "MonopolizeMote_Run");
}

// original 0x4EF3D0: +9 down; at 0 round the owner at its height, a colour
// from Monopolize_Colours; +0x14 0x80, +0x20 -8, +9 1, +0xA 0x10, +2 on.
S34_EXPORT void __cdecl MonopolizeMote_Launch(void) { LaunchMote(::bof3::Img(Monopolize_Colours_addr), 0, 0xFFFFFFF8u, 1, 0x10); }

// original 0x4EF4E0: the spiral at (+0xB & 0x1F) << 7; +9 up to 0x10; once
// there +0xA down, at 0 the mote freed.
S34_EXPORT void __cdecl MonopolizeMote_Fall(void) {
    SpiralMote((Sc()[0xB] & 0x1Fu) << 7);
    unsigned char* const s = Sc();
    if (s[9] != 0x10) {
        Inc(s[9]);
        return;
    }
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    FreeMote(s);
}

// original 0x4EF5C0: the first of Monopolize_Pool's 64 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Monopolize_PoolAlloc(void) { return PoolAlloc(::bof3::Img(Monopolize_Pool_addr), kMotePools, 0x84); }

void MonopolizeMote_Inject() {
    BOF3_INJECT(Monopolize_Task);
    BOF3_INJECT(Monopolize_Start);
    BOF3_INJECT(MonopolizeMote_Task);
    BOF3_INJECT(MonopolizeMote_Run);
    BOF3_INJECT(MonopolizeMote_Launch);
    BOF3_INJECT(MonopolizeMote_Fall);
    BOF3_INJECT(Monopolize_PoolAlloc);
}

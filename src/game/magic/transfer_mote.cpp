// Moved here by tools/move_funcs.py from battle_odds.cpp, magic_s34.cpp (docs/source-layout-round.md).
#include "game/magic/transfer_mote.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_odds_callees.h"
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

using namespace battle_odds;

}  // namespace

namespace {

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U SpriteCurrent() { return Long(AtBattleOdds(battle_odds::at::Sprite_CurrentAt())); }

unsigned char* Sprite() { return AtBattleOdds(SpriteCurrent()); }

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

constexpr unsigned kTransferRecords = 128;

constexpr unsigned kTransferStride = 0x2C;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Cur() { return Pointer(::bof3::Img(TransferMote_Current_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code), in the fuzz that
// address's recorder.
using Fn0 = void (__cdecl*)();

using Fn2 = void (__cdecl*)(unsigned, unsigned);

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_WORD(Fn2, address)(a, b); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// Linked at the current Transfer record's (+0x14, +0x18), layer 2.
void LinkAtCur(unsigned size) {
    const unsigned char* const r = Cur();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(LongMagicS34(r + 0x14)), static_cast<unsigned long>(LongMagicS34(r + 0x18)), 2,
                                size);
}

// A draw-mode packet (tpage 0x35, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

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

unsigned char* TransferRecord(unsigned index) { return Mem(::bof3::Img(Transfer_Pool_addr) + index * kTransferStride); }

short OffsetX(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(TransferMote_Offsets_addr) + 4 * (k & 0x1F)))); }

std::uint16_t OffsetXW(unsigned k) { return Word(Mem(::bof3::Img(TransferMote_Offsets_addr) + 4 * (k & 0x1F))); }

std::uint16_t OffsetYW(unsigned k) { return Word(Mem(::bof3::Img(TransferMote_Offsets_addr) + 4 * (k & 0x1F) + 2)); }

// The current record's +0xC up, its x +0x20 its base +8 plus the sine of
// (+0xC & 0x3F) << 6 x 24 >> 12, its y +0x22 on by +0xA.
void SwayMote() {
    AddWord(Cur() + 0xC, 1);
    const unsigned angle = (Cur()[0xC] & 0x3Fu) << 6;
    SetSW(6, angle);
    const int v = Sin(static_cast<short>(angle));
    unsigned char* const r = Cur();
    const int dx = static_cast<int>(static_cast<std::uint32_t>(v) * 24u) >> 12;
    SetWord(r + 0x20, (static_cast<unsigned>(dx) + Word(r + 8)) & 0xFFFF);
    AddWord(r + 0x22, Word(r + 0xA));
}

// Words 0x903850 / 52 the current record's point, 58 / 5A / 5C its +5 x 6;
// a draw-mode packet linked at the record.
void LinesHead() {
    {
        const unsigned char* const r = Cur();
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
    }
    DrawMode(0x35);
    LinkAtCur(0xC);
    const unsigned char* const r = Cur();
    const unsigned shade = r[5] * 6u;
    SetSW(8, shade);
    SetSW(0xA, shade);
    SetSW(0xC, shade);
}

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EE6C0: the kind-2 task. A six-entry stack table by +1
// (Transfer_Start, BattleFx_TintActor, BattleFx_Brighten, BattleFx_WaitStep4,
// Sparkle_End, BattleFx_Finish); then Transfer_Pool walked: every record with
// bit 0 of +0 becomes TransferMote_Current, its +0x28 the owner, for
// TransferMote_Task; the owner put back after each (the current pointer is
// not).
S34_EXPORT void __cdecl Transfer_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {addr::Transfer_Start,    addr::BattleFx_TintActor, addr::BattleFx_Brighten,
                                                 addr::BattleFx_WaitStep4, addr::Sparkle_End,        addr::BattleFx_Finish};
    Dispatch(kPhases, 6, Sc()[1], "Transfer_Task");
    const std::int32_t owner = LongMagicS34(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kTransferRecords; ++i) {
        unsigned char* const rec = TransferRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = LongMagicS34(rec + 0x28);
        SetLong(Mem(::bof3::Img(TransferMote_Current_addr)), static_cast<std::int32_t>(Key(rec)));
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(addr::TransferMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
    }
}

// original 0x4EE750: the pool's +0..+2 cleared; +4 4 (the kind); the task at
// the source sprite (its facing and point) and on the screen; +0xB 0, +9 8,
// +0xA 0, +1 on; sound 0x100; then Transfer_Counts[+4] records
// (Transfer_PoolAlloc; a full pool skips one), each owned by this task, +1 /
// +2 0, +3 Rand & 3, +4 the kind, +7 its number, +5 a delay of (number / 4)
// x Transfer_Delays[kind] + 1, counted in +0xB. Sprite_Current's +4 is read
// again at each use.
S34_EXPORT void __cdecl Transfer_Start(void) {
    for (unsigned i = 0; i < kTransferRecords; ++i) {
        unsigned char* const rec = TransferRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    Sc()[4] = 4;
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        s[8] = src[8];
        ts->x = rv::As<records::SpriteHead>(src)->x;
        ts->z = rv::As<records::SpriteHead>(src)->z;
        rv::Store(ts->height, rv::As<records::SpriteHead>(src)->height);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        unsigned char* const s = Sc();
        s[0xB] = 0;
        s[9] = 8;
        s[0xA] = 0;
        Inc(s[1]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    if (Mem(::bof3::Img(Transfer_Counts_addr) + Sc()[4])[0] == 0) return;
    unsigned char i = 0;
    do {
        const unsigned index = MH_AT(ByteFn, addr::Transfer_PoolAlloc)() & 0xFFu;
        if (index != 0xFF) {
            unsigned char* const rec = TransferRecord(index);
            SetLong(rec + 0x28, static_cast<std::int32_t>(Key(Sc())));
            rec[1] = 0;
            rec[2] = 0;
            const std::uint32_t r = RandCall();
            unsigned char* const self = Sc();
            rec[3] = static_cast<unsigned char>(r & 3);
            rec[4] = self[4];
            rec[7] = i;
            const unsigned char step = Mem(::bof3::Img(Transfer_Delays_addr) + self[4])[0];
            rec[5] = static_cast<unsigned char>(static_cast<unsigned>(i >> 2) * step + 1);
            Inc(self[0xB]);
        }
        ++i;
    } while (i < Mem(::bof3::Img(Transfer_Counts_addr) + Sc()[4])[0]);
}

// original 0x4EE8A0: once +0xB is 4 or below (unsigned), +9 = 8 and the next
// state. eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_WaitStep4(void) {
    unsigned char* s = Sprite();
    if (s[0xB] > 4) return Low32(s);
    s[9] = 8;
    s = Sprite();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    return Low32(s);
}

// original 0x4EE8C0: a record's task, a jmp through TransferMote_TaskTable
// (one entry) by the current record's +1, unchecked.
S34_EXPORT void __cdecl TransferMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TransferMote_Run};
    Dispatch(kKinds, 1, Cur()[1], "TransferMote_Task");
}

// original 0x4EE8E0: a three-entry stack table by the current record's +2
// (TransferMote_Start, _Rise, _Fade); while its +0 bit 0 and +2 are set the
// star, and when +0xC & 3 is not 0 the rays (Frame_Counter >> 1, +6) and the
// forks (Frame_Counter >> 1 + 4, +6 >> 1 + +6).
S34_EXPORT void __cdecl TransferMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::TransferMote_Start, addr::TransferMote_Rise, addr::TransferMote_Fade};
    Dispatch(kSteps, 3, Cur()[2], "TransferMote_Run");
    {
        const unsigned char* const r = Cur();
        if ((r[0] & 1) == 0 || r[2] == 0) return;
    }
    Call0(addr::TransferMote_DrawStar);
    if ((Cur()[0xC] & 3) == 0) return;
    Call2(addr::TransferMote_DrawRays, Frame_Counter >> 1, Cur()[6]);
    const unsigned char b = Cur()[6];
    Call2(addr::TransferMote_DrawForks, (Frame_Counter >> 1) + 4, (static_cast<unsigned>(b >> 1) + b) & 0xFFFF);
}

// original 0x4EE970: +5 down; at 0 the record at the owner's point (+0x14..
// +0x1C) and screen point (+0x20, +0x22), moved by
// TransferMote_Offsets[+7 & 0x1F] and Rand & 7 (away from 0 in x, up in y);
// +8 the x, +0xA Rand & 1, +0xC Rand, +7 (Rand & 6) + Transfer_Rise[+4], +5
// 0x10, +6 0, +2 on.
S34_EXPORT void __cdecl TransferMote_Start(void) {
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    {
        SetLong(Cur() + 0x14, rv::As<records::SpriteHead>(Owner())->x);
        SetLong(Cur() + 0x18, rv::As<records::SpriteHead>(Owner())->z);
        SetLong(Cur() + 0x1C, rv::As<records::SpriteHead>(Owner())->height);
        SetWord(Cur() + 0x20, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
        SetWord(Cur() + 0x22, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    }
    unsigned char* at = Cur() + 0x20;
    if (OffsetX(Cur()[7]) < 0) {
        const std::uint32_t r = RandCall();
        const unsigned d = (OffsetXW(Cur()[7]) - (r & 7u)) & 0xFFFF;
        AddWord(at, d);
    } else {
        const std::uint32_t r = RandCall();
        const unsigned d = ((r & 7u) + OffsetXW(Cur()[7])) & 0xFFFF;
        AddWord(at, d);
    }
    at = Cur() + 0x22;
    {
        const std::uint32_t r = RandCall();
        const unsigned d = ((r & 7u) + OffsetYW(Cur()[7])) & 0xFFFF;
        SetWord(at, (Word(at) - d) & 0xFFFF);
    }
    SetWord(Cur() + 8, Word(Cur() + 0x20));
    {
        const std::uint32_t r = RandCall();
        SetWord(Cur() + 0xA, r & 1);
    }
    {
        const std::uint32_t r = RandCall();
        SetWord(Cur() + 0xC, r & 0xFFFF);
    }
    const std::uint32_t r = RandCall();
    unsigned char* const rec = Cur();
    rec[7] = static_cast<unsigned char>((r & 6) + Mem(::bof3::Img(Transfer_Rise_addr) + rec[4])[0]);
    rec[5] = 0x10;
    rec[6] = 0;
    Inc(rec[2]);
}

// original 0x4EEAE0: the sway (SwayMote); +6 up, at +7 +2 on.
S34_EXPORT void __cdecl TransferMote_Rise(void) {
    SwayMote();
    unsigned char* const r = Cur();
    Inc(r[6]);
    if (r[6] == r[7]) Inc(r[2]);
}

// original 0x4EEB50: the sway; every fourth frame +6 down; +5 down, at 0 the
// owner's count +0xB down and the record freed (a tail jmp).
S34_EXPORT void __cdecl TransferMote_Fade(void) {
    SwayMote();
    if ((static_cast<unsigned char>(Frame_Counter) & 3) == 0) Dec(Cur()[6]);
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::TransferMote_Free);
}

// original 0x4EEBE0: four semi-transparent gouraud lines (Gpu_SetLineG2)
// from the record's screen point at the angles (a1 + 8 i & 0x1F) << 7 for
// i < 4, length a2 (both u16), shaded +5 x 6 at the point and 1 at the end;
// after a draw-mode packet (tpage 0x35), all linked at the record.
S34_EXPORT void __cdecl TransferMote_DrawRays(unsigned a1, unsigned a2) {
    LinesHead();
    const unsigned from = a1 & 0xFFFF;
    const unsigned to = from + 0x20;
    const int length = static_cast<int>(a2 & 0xFFFF);
    for (unsigned k = from; k < to; k += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SS(0);
        SetSW(6, (k & 0x1Fu) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = Cos(SS(6));
        PutFloat(p + 0x18, Mul12(v, length) + SS(0));
        v = Sin(SS(6));
        PutFloat(p + 0x1C, Mul12(v, length) + SS(2));
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        LinkAtCur(0x34);
    }
}

// original 0x4EED70: four three-point gouraud lines (Gpu_SetLineG3) from the
// record's screen point at the angles (a1 + 8 i & 0x1F) << 7, to a2 / 2 and
// a2 along it (both u16), shaded 1, +5 x 6, 1; after a draw-mode packet
// (tpage 0x35), all linked at the record.
S34_EXPORT void __cdecl TransferMote_DrawForks(unsigned a1, unsigned a2) {
    LinesHead();
    const unsigned from = a1 & 0xFFFF;
    const unsigned to = from + 0x20;
    const int outer = static_cast<int>(a2 & 0xFFFF);
    const int inner = outer >> 1;
    for (unsigned k = from; k < to; k += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SS(0);
        SetSW(6, (k & 0x1Fu) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = Cos(SS(6));
        PutFloat(p + 0x18, Mul12(v, inner) + SS(0));
        v = Sin(SS(6));
        PutFloat(p + 0x1C, Mul12(v, inner) + SS(2));
        v = Cos(SS(6));
        PutFloat(p + 0x28, Mul12(v, outer) + SS(0));
        v = Sin(SS(6));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, Mul12(v, outer) + SS(2));
        p[0x14] = SB(8);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        LinkAtCur(0x34);
    }
}

// original 0x4EEF60: after a draw-mode packet (tpage 0x35), a star of eight
// semi-transparent gouraud triangles (Gpu_SetPolyG3) round the record's
// screen point at radius +6 + (Rand & 3), each 0x200 wide; the centre shaded
// TransferMote_Colours[+4 x 4 + +3] x +5, the rim +5; all linked at the
// record.
S34_EXPORT void __cdecl TransferMote_DrawStar(void) {
    DrawMode(0x35);
    LinkAtCur(0xC);
    const std::uint32_t rnd = RandCall();
    {
        const unsigned char* const r = Cur();
        SetSW(4, (rnd & 3u) + r[6]);
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
        const unsigned index = r[3] + r[4] * 4u;
        const unsigned char* const c = Mem(::bof3::Img(TransferMote_Colours_addr) + index * 3u);
        SetSW(8, c[0] * static_cast<unsigned>(r[5]));
        SetSW(0xA, c[1] * static_cast<unsigned>(r[5]));
        SetSW(0xC, c[2] * static_cast<unsigned>(r[5]));
        SetSW(0xE, r[5]);
    }
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = Sin(a);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = Cos(a);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        a += 0x200;
        v = Sin(a);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = Cos(a);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xE);
        LinkAtCur(0x34);
    }
}

// original 0x4EF190: the first of Transfer_Pool's 128 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Transfer_PoolAlloc(void) {
    return PoolAlloc(::bof3::Img(Transfer_Pool_addr), kTransferRecords, kTransferStride);
}

// original 0x4EF1E0: the current record's +0..+4 cleared.
S34_EXPORT void __cdecl TransferMote_Free(void) {
    unsigned char* const r = Cur();
    r[0] = 0;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
    r[4] = 0;
}

void TransferMote_Inject() {
    BOF3_INJECT(Transfer_Task);
    BOF3_INJECT(Transfer_Start);
    BOF3_INJECT(BattleFx_WaitStep4);
    BOF3_INJECT(TransferMote_Task);
    BOF3_INJECT(TransferMote_Run);
    BOF3_INJECT(TransferMote_Start);
    BOF3_INJECT(TransferMote_Rise);
    BOF3_INJECT(TransferMote_Fade);
    BOF3_INJECT(TransferMote_DrawRays);
    BOF3_INJECT(TransferMote_DrawForks);
    BOF3_INJECT(TransferMote_DrawStar);
    BOF3_INJECT(Transfer_PoolAlloc);
    BOF3_INJECT(TransferMote_Free);
}

// Moved here by tools/move_funcs.py from magic_s35.cpp (docs/source-layout-round.md).
#include "game/magic/cure_mote.h"

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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// MAGIC168's pool: 128 motes of 0x2C bytes, the one being run at 0x6AFF10.
// kCurePool: now CureMote_Pool (symbols.toml).
constexpr std::uint32_t kCureStride = 0x2C;

constexpr unsigned kCureCount = 0x80;

constexpr ::bof3::image::Va kCureCurrent{0x6AFF10};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Cur() { return Pointer(::bof3::Img(CureMote_Current_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fst(p) dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* CureRecord(unsigned n) { return Mem(::bof3::Img(CureMote_Pool_addr) + n * kCureStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn2 = void (__cdecl*)(unsigned, unsigned);

using FnAlloc = std::uint32_t (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_WORD(Fn2, address)(a, b); }

unsigned Alloc(std::uint32_t address) { return MH_WORD(FnAlloc, address)() & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

// Linked at a MAGIC168 mote's +0x14 / +0x18.
void LinkCure(int dy, unsigned size) {
    const unsigned char* const r = Cur();
    Link(Long(r + 0x14), Long(r + 0x18), dy, size);
}

}  // namespace

#define S35_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// CureMote_Rise / _Fade's common half: word +0xC up; word 0x903856 (+0xC &
// 0x3F) << 6; word +0x20 word +8 + Math_Sin of it x 24 >> 12; word +0x22 +=
// word +0xA.
void CureMoteSway() {
    SetWord(Cur() + 0xC, Word(Cur() + 0xC) + 1u);
    const unsigned angle = (Cur()[0xC] & 0x3Fu) << 6;
    SetSW(6, angle);
    const int sn = MH_CALL(Math_Sin)(static_cast<short>(angle));
    {
        unsigned char* const r = Cur();
        SetWord(r + 0x20, static_cast<unsigned>(Mul12(sn, 24)) + Word(r + 8));
    }
    {
        unsigned char* const r = Cur();
        SetWord(r + 0x22, Word(r + 0x22) + Word(r + 0xA));
    }
}

// The mote's screen point into words 0x903850 / 0x903852, a draw-mode packet
// linked at it, and words 0x903858 / 5A / 5C its +5 x 6.
void CureMoteBegin() {
    SetSW(0, Word(Cur() + 0x20));
    SetSW(2, Word(Cur() + 0x22));
    DrawMode(0x35);
    LinkCure(2, 0xC);
}

void CureMoteShade() {
    const unsigned char* const r = Cur();
    SetSW(8, r[5] * 6u);
    SetSW(0xA, r[5] * 6u);
    SetSW(0xC, r[5] * 6u);
}

}  // namespace

// original 0x4F0670: the kind-2 task. A six-entry stack table by +1:
// Cure_Start, BattleFx_TintActor, BattleFx_Brighten, BattleFx_WaitStep4,
// Sparkle_End, BattleFx_Finish. Then every mote of the pool with bit 0 is run
// through CureMote_Task with 0x6AFF10 at it and its +0x28 as the owner cell
// 0x93B940, the owner (read once after the step) put back after each.
S35_EXPORT void __cdecl Cure_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Cure_Start,         bof3::addr::BattleFx_TintActor,
                                                 bof3::addr::BattleFx_Brighten,  bof3::addr::BattleFx_WaitStep4,
                                                 bof3::addr::Sparkle_End,        bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 6) PastTable("Cure_Task", phase, 6);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t mote_owner = Long(r + 0x28);
        magic_harness::SetPointer(::bof3::Img(kCureCurrent.v), r);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), mote_owner);
        Call0(bof3::addr::CureMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
    }
}

// original 0x4F0700: every mote's +0 / +1 / +2 cleared; +4 3 (the kind); the
// source sprite's +8, +0x34, +0x38, +0x3C to the task;
// BattleActor_UpdateScreenXY; +0xB 0, +9 8, +0xA 0, +1 on; sound 0x100. Then
// Cure_MoteCounts[+4] motes (CureMote_Alloc; none free: skipped): +0x28 the
// task, +1 / +2 0, +3 Rand & 3, +4 the kind, +7 the number n, +5
// Cure_MoteDelays[kind] x (n >> 2) + 1 (bytes); each counted in +0xB.
S35_EXPORT void __cdecl Cure_Start(void) {
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    Sc()[4] = 3;
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        Sc()[8] = src[8];
        rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
        rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
        rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayById)(0x100);
    if (Mem(::bof3::Img(Cure_MoteCounts_addr))[Sc()[4]] == 0) return;
    unsigned char n = 0;
    do {
        const unsigned slot = Alloc(bof3::addr::CureMote_Alloc);
        if (slot != 0xFF) {
            unsigned char* const r = CureRecord(slot);
            SetLong(r + 0x28, static_cast<std::int32_t>(Key(Sc())));
            r[1] = 0;
            r[2] = 0;
            const std::uint32_t rnd = RandCall();
            unsigned char* const s = Sc();
            r[3] = static_cast<unsigned char>(rnd & 3);
            r[4] = s[4];
            r[7] = n;
            r[5] = static_cast<unsigned char>(Mem(::bof3::Img(Cure_MoteDelays_addr))[s[4]] * (n >> 2) + 1);
            Inc(s[0xB]);
        }
        ++n;
    } while (n < Mem(::bof3::Img(Cure_MoteCounts_addr))[Sc()[4]]);
}

// original 0x4F0850: a mote's task, a jmp through CureMote_TaskTable (one
// entry) by the current mote's +1, unchecked.
S35_EXPORT void __cdecl CureMote_Task(void) {
    const unsigned phase = Cur()[1];
    if (phase >= 1) PastTable("CureMote_Task", phase, 1);
    magic_harness::Phase(bof3::addr::CureMote_Run)();
}

// original 0x4F0870: a three-entry stack table by the mote's +2 (CureMote_Wait,
// _Rise, _Fade); then while the mote's bit 0 and +2 are set: its star, and
// while +0xC & 3: its rays (Frame_Counter >> 1, radius +6) and arcs
// ((Frame_Counter >> 1) + 4, radius +6 + +6 / 2).
S35_EXPORT void __cdecl CureMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::CureMote_Wait, bof3::addr::CureMote_Rise,
                                                bof3::addr::CureMote_Fade};
    const unsigned phase = Cur()[2];
    if (phase >= 3) PastTable("CureMote_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const r = Cur();
        if ((r[0] & 1) == 0 || r[2] == 0) return;
    }
    Call0(bof3::addr::CureMote_DrawStar);
    if ((Cur()[0xC] & 3) == 0) return;
    Call2(bof3::addr::CureMote_DrawRays, Frame_Counter >> 1, Cur()[6]);
    const unsigned w = Cur()[6];
    Call2(bof3::addr::CureMote_DrawArcs, (Frame_Counter >> 1) + 4, (w >> 1) + w);
}

// original 0x4F0900: the mote's +5 down; at 0 it starts at its owner: +0x14 /
// +0x18 / +0x1C the owner's +0x34 / +0x38 / +0x3C, words +0x20 / +0x22 the
// owner's screen point +0x2E / +0x30, moved by CureMote_Offsets[+7 & 0x1F]
// (x away from 0 by Rand & 7, y up by it plus Rand & 7); word +8 the x, word
// +0xA Rand & 1, word +0xC Rand; +7 (Rand & 6) + CureMote_Lifetimes[+4], +5
// 0x10, +6 0, +2 on. Each Rand's pointer taken before the call, each table
// read after it.
S35_EXPORT void __cdecl CureMote_Wait(void) {
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    SetLong(Cur() + 0x14, rv::As<records::SpriteHead>(Owner())->x);
    SetLong(Cur() + 0x18, rv::As<records::SpriteHead>(Owner())->z);
    SetLong(Cur() + 0x1C, rv::As<records::SpriteHead>(Owner())->height);
    SetWord(Cur() + 0x20, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
    SetWord(Cur() + 0x22, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    {
        unsigned char* const x = Cur() + 0x20;
        const short dx = static_cast<short>(Word(Mem(::bof3::Img(CureMote_Offsets_addr) + (Cur()[7] & 0x1Fu) * 4)));
        if (dx < 0) {
            const std::uint32_t rnd = RandCall() & 7;
            const std::uint16_t d = Word(Mem(::bof3::Img(CureMote_Offsets_addr) + (Cur()[7] & 0x1Fu) * 4));
            SetWord(x, Word(x) + static_cast<std::uint16_t>(d - rnd));
        } else {
            const std::uint32_t rnd = RandCall() & 7;
            const std::uint16_t d = Word(Mem(::bof3::Img(CureMote_Offsets_addr) + (Cur()[7] & 0x1Fu) * 4));
            SetWord(x, Word(x) + static_cast<std::uint16_t>(rnd + d));
        }
    }
    {
        unsigned char* const y = Cur() + 0x22;
        const std::uint32_t rnd = RandCall() & 7;
        const std::uint16_t d = Word(Mem(::bof3::Img(CureMote_Offsets_addr) + (Cur()[7] & 0x1Fu) * 4 + 2));
        SetWord(y, Word(y) - static_cast<std::uint16_t>(rnd + d));
    }
    SetWord(Cur() + 8, Word(Cur() + 0x20));
    {
        const std::uint32_t rnd = RandCall();
        SetWord(Cur() + 0xA, rnd & 1);
    }
    {
        const std::uint32_t rnd = RandCall();
        SetWord(Cur() + 0xC, rnd);
    }
    {
        const std::uint32_t rnd = RandCall();
        unsigned char* const r = Cur();
        r[7] = static_cast<unsigned char>((rnd & 6) + Mem(::bof3::Img(CureMote_Lifetimes_addr))[r[4]]);
    }
    Cur()[5] = 0x10;
    Cur()[6] = 0;
    Inc(Cur()[2]);
}

// original 0x4F0A70: the sway; +6 up; at +7 +2 on.
S35_EXPORT void __cdecl CureMote_Rise(void) {
    CureMoteSway();
    Inc(Cur()[6]);
    if (Cur()[6] == Cur()[7]) Inc(Cur()[2]);
}

// original 0x4F0AE0: the sway; +6 down every fourth frame; +5 down; at 0 the
// owner's +0xB down and a tail jmp to CureMote_Free.
S35_EXPORT void __cdecl CureMote_Fade(void) {
    CureMoteSway();
    if ((Frame_Counter & 3) == 0) Dec(Cur()[6]);
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::CureMote_Free);
}

// original 0x4F0B70 (cdecl: a, radius, each read as a u16): four
// semi-transparent gouraud lines from the mote's screen point outwards at the
// angles (a + 8 j & 0x1F) << 7, the length radius; the inner end shaded +5 x
// 6, the outer end (1, 1, 1).
S35_EXPORT void __cdecl CureMote_DrawRays(unsigned a, unsigned radius) {
    CureMoteBegin();
    const unsigned first = a & 0xFFFF;
    CureMoteShade();
    const unsigned end = first + 0x20;
    const int length = static_cast<int>(radius & 0xFFFF);
    for (unsigned j = first; j < end; j += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, (j & 0x1F) << 7);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x18, Mul12(cs, length) + SS(0));
        }
        {
            const int sn = MH_CALL(Math_Sin)(SS(6));
            PutFloat(p + 0x1C, Mul12(sn, length) + SS(2));
        }
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        LinkCure(2, 0x34);
    }
}

// original 0x4F0D00 (cdecl: a, radius, each read as a u16): four
// semi-transparent three-point gouraud lines from the mote's screen point
// outwards at the angles (a + 8 j & 0x1F) << 7, through radius / 2 to radius;
// the middle point shaded +5 x 6, the ends (1, 1, 1).
S35_EXPORT void __cdecl CureMote_DrawArcs(unsigned a, unsigned radius) {
    CureMoteBegin();
    const unsigned first = a & 0xFFFF;
    CureMoteShade();
    const unsigned end = first + 0x20;
    const int outer = static_cast<int>(radius & 0xFFFF);
    const int middle = outer >> 1;
    for (unsigned j = first; j < end; j += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, (j & 0x1F) << 7);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x18, Mul12(cs, middle) + SS(0));
        }
        {
            const int sn = MH_CALL(Math_Sin)(SS(6));
            PutFloat(p + 0x1C, Mul12(sn, middle) + SS(2));
        }
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x28, Mul12(cs, outer) + SS(0));
        }
        const int sn = MH_CALL(Math_Sin)(SS(6));
        const int y2 = Mul12(sn, outer) + SS(2);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, y2);
        p[0x14] = SB(8);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        LinkCure(2, 0x34);
    }
}

// original 0x4F0EF0: an eight-point star of semi-transparent gouraud triangles
// round the mote's screen point, the radius +6 + Rand & 3 (word 0x903854):
// the centre CureMote_Colours[kind x 4 + variant] x +5, the rim +5 in all
// three channels; under a draw-mode packet.
S35_EXPORT void __cdecl CureMote_DrawStar(void) {
    DrawMode(0x35);
    LinkCure(2, 0xC);
    {
        const std::uint32_t rnd = RandCall();
        const unsigned char* const r = Cur();
        SetSW(4, (rnd & 3) + r[6]);
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
        const std::uint32_t colour = ::bof3::Img(CureMote_Colours_addr) + (r[3] + r[4] * 4u) * 3u;
        SetSW(8, Mem(colour)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xA, Mem(colour + 1)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xC, Mem(colour + 2)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xE, r[5]);
    }
    for (int angle = 0; angle < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int sn = MH_CALL(Math_Sin)(angle);
            PutFloat(p + 0x18, Mul12(sn, SS(4)) + SS(0));
        }
        {
            const int cs = MH_CALL(Math_Cos)(angle);
            PutFloat(p + 0x1C, Mul12(cs, SS(4)) + SS(2));
        }
        angle += 0x200;
        {
            const int sn = MH_CALL(Math_Sin)(angle);
            PutFloat(p + 0x28, Mul12(sn, SS(4)) + SS(0));
        }
        {
            const int cs = MH_CALL(Math_Cos)(angle);
            PutFloat(p + 0x2C, Mul12(cs, SS(4)) + SS(2));
        }
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xE);
        LinkCure(2, 0x34);
    }
}

// original 0x4F1120: the first of the 128 motes without bit 0, which it sets;
// its number in al, or 0xFF when none is free.
S35_EXPORT unsigned __cdecl CureMote_Alloc(void) {
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        if ((r[0] & 1) == 0) {
            r[0] |= 1;
            return n;
        }
    }
    return 0xFF;
}

// original 0x4F1170: the current mote's +0 .. +4 cleared.
S35_EXPORT void __cdecl CureMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
}

void CureMote_Inject() {
    BOF3_INJECT(Cure_Task);
    BOF3_INJECT(Cure_Start);
    BOF3_INJECT(CureMote_Task);
    BOF3_INJECT(CureMote_Run);
    BOF3_INJECT(CureMote_Wait);
    BOF3_INJECT(CureMote_Rise);
    BOF3_INJECT(CureMote_Fade);
    BOF3_INJECT(CureMote_DrawRays);
    BOF3_INJECT(CureMote_DrawArcs);
    BOF3_INJECT(CureMote_DrawStar);
    BOF3_INJECT(CureMote_Alloc);
    BOF3_INJECT(CureMote_Free);
}

// Moved here by tools/move_funcs.py from magic_s35.cpp (docs/source-layout-round.md).
#include "game/magic/benediction_child.h"

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

constexpr ::bof3::image::Va kPartyCount{0x904AB0};    // u8: the party's size

// MAGIC169's pool: 80 motes of 0x84 bytes, run as tasks (+0x80 the owner).
// kBlessPool: now BenedictionMote_Pool (symbols.toml).
constexpr std::uint32_t kBlessStride = 0x84;

constexpr unsigned kBlessCount = 0x50;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::int32_t Mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

std::int32_t Add32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `cdq / and edx, 2^n - 1 / add / sar n` (or `cdq / sub eax, edx / sar 1`):
// a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `fild dword` then `fst(p) dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the party records by a byte, unchecked.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* BlessRecord(unsigned n) { return Mem(::bof3::Img(BenedictionMote_Pool_addr) + n * kBlessStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using FnAlloc = std::uint32_t (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

unsigned Alloc(std::uint32_t address) { return MH_WORD(FnAlloc, address)() & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

int SinSS(int angle, unsigned k) {
    const int v = MH_CALL(Math_Sin)(angle);
    return Mul12(v, SS(k));
}

int CosSS(int angle, unsigned k) {
    const int v = MH_CALL(Math_Cos)(angle);
    return Mul12(v, SS(k));
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

// Linked at the sprite's +0x34 / +0x38 (the task's or the mote's).
void LinkAt(const unsigned char* at, int dy, unsigned size) { Link(rv::As<records::SpriteHead>(at)->x, rv::As<records::SpriteHead>(at)->z, dy, size); }

}  // namespace

#define S35_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The orbit of BenedictionMote_Launch / _Spiral / _Fade (ReviveMote_Launch's,
// group S17): word 0x903854 (+0xB & 0xF) << 8; +0x34 / +0x38 the owner's plus
// Math_Sin / Math_Cos of it x dword +0xC.
void BlessMoteOrbit() {
    const unsigned angle = (Sc()[0xB] & 0xFu) << 8;
    SetSW(4, angle);
    {
        const int sn = MH_CALL(Math_Sin)(static_cast<short>(angle));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = Add32(Mul32(sn, ts->step_x), rv::As<records::SpriteHead>(Owner())->x);
    }
    {
        const int cs = MH_CALL(Math_Cos)(SS(4));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->z = Add32(Mul32(cs, ts->step_x), rv::As<records::SpriteHead>(Owner())->z);
    }
}

// The rise: while dword +0x20 is below +0x14 (signed), +0x14 -= +0x20 and
// +0x3C += +0x14.
void BlessMoteRise() {
    unsigned char* const s = Sc();
    const std::int32_t d = rv::As<records::BattleTask>(s)->work_20;
    const std::int32_t h = rv::As<records::BattleTask>(s)->step_y;
    if (d >= h) return;
    rv::Store(rv::As<records::BattleTask>(s)->step_y, Add32(h, -d));
    rv::Store(rv::Current<records::BattleTask>()->height, Add32(rv::Current<records::BattleTask>()->height, rv::Current<records::BattleTask>()->step_y));
}

}  // namespace

// original 0x4F11A0: the kind-2 task. A four-entry stack table by +1:
// Benediction_Start, _Spawn, _Wait, MagicFx_DoneAndFree. Then while +0 and +1
// are set: BattleActor_UpdateScreenXY and the halo. Then every mote of the
// second pool with bit 0 is run through BenedictionMote_Task as
// Sprite_Current, its +0x80 as the owner cell; both put back after each (the
// owner as read before the loop, Sprite_Current as read after the halo).
S35_EXPORT void __cdecl Benediction_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Benediction_Start, bof3::addr::Benediction_Spawn,
                                                 bof3::addr::Benediction_Wait, bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Benediction_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* task = Sc();
    if (task[0] != 0 && task[1] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::Benediction_DrawHalo);
        task = Sc();
    }
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        records::BattleTask* const tr = rv::As<records::BattleTask>(r);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t mote_owner = static_cast<std::int32_t>(tr->owner);
        Sprite_Current = r;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), mote_owner);
        Call0(bof3::addr::BenedictionMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = task;
    }
}

// original 0x4F1250: every mote's +0 / +1 / +2 cleared; MagicFx_CenterOnSide;
// +0x3C up by 0xC00000; Gfx_ClutStrip row 26 from its source with bit 15 set,
// then its first word again without it, Gfx_ClutStripDirty 1; sound 0x100; +9
// and +0xB 0, +1 on.
S35_EXPORT void __cdecl Benediction_Start(void) {
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    rv::Store(rv::Current<records::BattleTask>()->height, Add32(rv::Current<records::BattleTask>()->height, 0xC00000));
    for (unsigned k = 0x1A00; k < 0x1B00; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
}

// original 0x4F12E0: +9 up; at 0x10: when the target byte has 0x80 (the party
// side), one child (kind 1, 8) per party member (0x904AB0, read again each
// time): +0x80 the task, +1 0, +4 the member, +9 1 + 0x28 n, each counted in
// +0xB; then +1 on.
S35_EXPORT void __cdecl Benediction_Spawn(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    if ((TargetByte() & 0x80) != 0 && Mem(::bof3::Img(kPartyCount.v))[0] != 0) {
        unsigned char delay = 1;
        unsigned char n = 0;
        do {
            unsigned char* const c = TaskSlot(NewTask(8));
            records::BattleTask* const tc = rv::As<records::BattleTask>(c);
            unsigned char* const s = Sc();
            tc->owner = static_cast<std::int32_t>(Key(s));
            c[1] = 0;
            c[4] = n;
            c[9] = delay;
            Inc(s[0xB]);
            AddB(delay, 0x28);
            ++n;
        } while (n < Mem(::bof3::Img(kPartyCount.v))[0]);
    }
    Inc(Sc()[1]);
}

// original 0x4F1390: once +0xB is 0 (every child ended): +9 down, at 0 +1 on.
S35_EXPORT void __cdecl Benediction_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4F13C0: one semi-transparent flat-textured quad (tpage
// Gpu_GetTPage(1, 1, 0x340, 0x100), CLUT Gpu_GetClut(0, 0x1FA), texels (8, 8)
// .. (0x78, 0x68)), 0x70 wide and 0x60 high above the task's screen point, its
// shade +9 << 3 (word 0x903856); under a draw-mode packet (tpage 0xB5), both
// linked at the task's +0x34 / +0x38 layer 1.
S35_EXPORT void __cdecl Benediction_DrawHalo(void) {
    DrawMode(0xB5);
    LinkAt(Sc(), 1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    int x, y;
    {
        const unsigned char* const s = Sc();
        SetSW(6, static_cast<unsigned>(s[9]) << 3);
        x = S16(s + 0x2E);
        y = S16(s + 0x30);
    }
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x - 0x38);
    PutFloat(p + 0xC, y - 0x60);
    PutFloat(p + 0x18, x + 0x38);
    PutFloat(p + 0x1C, y - 0x60);
    PutFloat(p + 0x38, x + 0x38);
    PutFloat(p + 0x28, x - 0x38);
    PutFloat(p + 0x2C, y);
    PutFloat(p + 0x3C, y);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 8;
    p[0x15] = 8;
    p[0x25] = 8;
    p[0x34] = 8;
    p[0x24] = 0x78;
    p[0x35] = 0x68;
    p[0x44] = 0x78;
    p[0x45] = 0x68;
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    LinkAt(Sc(), 1, 0x48);
}

// original 0x4F1500: the child's kind-1 task (parameter 8), a jmp through
// BenedictionChild_TaskTable (one entry) by +1, unchecked.
S35_EXPORT void __cdecl BenedictionChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BenedictionChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BenedictionChild_Run)();
}

// original 0x4F1520: a six-entry stack table by +2: BenedictionChild_Start,
// _Tint, _Brighten, MAGIC073's ActorFx_WaitStep4, _Fade, _End.
S35_EXPORT void __cdecl BenedictionChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::BenedictionChild_Start,    bof3::addr::BenedictionChild_Tint,
                                                bof3::addr::BenedictionChild_Brighten, bof3::addr::ActorFx_WaitStep4,
                                                bof3::addr::BenedictionChild_Fade,     bof3::addr::BenedictionChild_End};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("BenedictionChild_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4F1570: +9 down; at 0: the party member +4's (0x802D40 + 0x14C x
// +4, unchecked) +8, +0x34, +0x38, +0x3C to the task; +0xB 0, +9 8, +0xA 0, +2
// on; then 24 motes of the second pool (BenedictionMote_Alloc; none free:
// skipped): +0x80 the task, +1 0, +0xB the number n, +9 (n >> 2) + 0x10, each
// counted in +0xB.
S35_EXPORT void __cdecl BenedictionChild_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        const unsigned char* const m = PartyRecord(Sc()[4]);
        const records::PartyMember* const tm = rv::AsConst<records::PartyMember>(m);
        Sc()[8] = m[8];
        rv::Store(rv::Current<records::BattleTask>()->x, tm->x);
        rv::Store(rv::Current<records::BattleTask>()->z, tm->z);
        rv::Store(rv::Current<records::BattleTask>()->height, tm->height);
    }
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    for (unsigned n = 0; n < 0x18; ++n) {
        const unsigned slot = Alloc(bof3::addr::BenedictionMote_Alloc);
        if (slot == 0xFF) continue;
        unsigned char* const r = BlessRecord(slot);
        records::BattleTask* const tr = rv::As<records::BattleTask>(r);
        unsigned char* const s = Sc();
        tr->owner = static_cast<std::int32_t>(Key(s));
        r[1] = 0;
        r[0xB] = static_cast<unsigned char>(n);
        r[9] = static_cast<unsigned char>((n >> 2) + 0x10);
        Inc(s[0xB]);
    }
}

// original 0x4F1660: +9 down; at 0 the member's sprite record (taken before
// the count) released and tinted black (Sprite_SetTint(r, 0, 0, 0, 1)), the
// tint slot to +0xA, +9 8, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Tint(void) {
    unsigned char* const m = PartyRecord(Sc()[4]);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(m);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(m, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F16D0: the tint record +0xA (MoveScript_TintRecords + 12 n)
// one step brighter in +2..+4; +9 down; at 0 sound 0x101 (odd member) or
// 0x102, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)((Sc()[4] & 1) != 0 ? 0x101 : 0x102);
    Inc(Sc()[2]);
}

// original 0x4F1760: the tint record one step darker; +9 down; at 0 the
// member's sprite record (taken at entry) released, the member flashed, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Fade(void) {
    unsigned char* const s = Sc();
    unsigned char* const m = PartyRecord(s[4]);
    for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(m);
    MH_CALL(BattleActor_Flash)(Sc()[4]);
    Inc(Sc()[2]);
}

// original 0x4F1800: once +0xB is 0 (every mote ended): the owner's +0xB
// down; when Battle_ActorIsOut(+4) answers non-zero, Battle_SetTargetFlag40(+4);
// a tail jmp to BattleTask_FreeCurrent.
S35_EXPORT void __cdecl BenedictionChild_End(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    if (MH_CALL(Battle_ActorIsOut)(Sc()[4]) != 0) MH_CALL(Battle_SetTargetFlag40)(Sc()[4]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F1850: a mote's task, a jmp through BenedictionMote_TaskTable
// (one entry) by +1, unchecked.
S35_EXPORT void __cdecl BenedictionMote_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BenedictionMote_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BenedictionMote_Run)();
}

// original 0x4F1870: a three-entry stack table by +2 (BenedictionMote_Launch,
// _Spiral, _Fade); a draw-mode packet (tpage 0x35) linked at +0x34 / +0x38;
// while +0 and +2 are set: BattleActor_UpdateScreenXY, MAGIC077's
// ReviveMote_Draw and the glow; a closing draw-mode packet (tpage 0x15).
S35_EXPORT void __cdecl BenedictionMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BenedictionMote_Launch, bof3::addr::BenedictionMote_Spiral,
                                                bof3::addr::BenedictionMote_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("BenedictionMote_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    DrawMode(0x35);
    LinkAt(Sc(), 2, 0xC);
    {
        const unsigned char* const s = Sc();
        if (s[0] != 0 && s[2] != 0) {
            MH_CALL(BattleActor_UpdateScreenXY)();
            MH_CALL(ReviveMote_Draw)();
            Call0(bof3::addr::BenedictionMote_DrawGlow);
        }
    }
    DrawMode(0x15);
    LinkAt(Sc(), 2, 0xC);
}

// original 0x4F1920: +9 down; at 0: dword +0xC 8, the orbit, +0x3C the owner's
// + 0x800000, +0x14 ((Rand & 3) + 2) << 20, +0x20 +0x14 / 16, +0x5D / 5E / 5F
// (Rand & 7) + 6 each, +9 0, +0xA 0x10, +2 on.
S35_EXPORT void __cdecl BenedictionMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Current<records::BattleTask>()->step_x = 8;
    BlessMoteOrbit();
    rv::Store(rv::Current<records::BattleTask>()->height, Add32(rv::As<records::SpriteHead>(Owner())->height, 0x800000));
    {
        const std::uint32_t rnd = RandCall();
        rv::Store(rv::Current<records::BattleTask>()->step_y, static_cast<std::int32_t>(((rnd & 3) + 2) << 20));
    }
    rv::Store(rv::Current<records::BattleTask>()->work_20, DivPow2(rv::Current<records::BattleTask>()->step_y, 4));
    for (unsigned k = 0x5D; k < 0x60; ++k) {
        const std::uint32_t rnd = RandCall();
        Sc()[k] = static_cast<unsigned char>((rnd & 7) + 6);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F1A40: dword +0xC up by 2, the orbit, the rise; +9 up by 2, at
// 0x10 +2 on. Also MAGIC077's (ReviveMote_Task's entry 1).
S35_EXPORT void __cdecl BenedictionMote_Spiral(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, Add32(rv::Current<records::BattleTask>()->step_x, 2));
    BlessMoteOrbit();
    BlessMoteRise();
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4F1B00: dword +0xC up, the orbit, the rise; on odd frames +9
// down, at 0 the owner's +0xB down and a tail jmp to MAGIC219's 0x4F6290 (the
// pool slot's free).
S35_EXPORT void __cdecl BenedictionMote_Fade(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, Add32(rv::Current<records::BattleTask>()->step_x, 1));
    BlessMoteOrbit();
    BlessMoteRise();
    if ((Frame_Counter & 1) == 0) return;
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F1BD0: a ring of eight semi-transparent gouraud quads round the
// sprite's screen point (+0x2E / +0x30, read once) between radius 0x14 (word
// 0x903852) and 8 (word 0x903850): the outer edge (1, 1, 1), the inner edge
// the signed bytes +0x5D / 5E / 5F x +9 (words 0x903858 / 5A / 5C); each
// linked at +0x34 / +0x38 layer 2. Also MAGIC077's (ReviveMote_Draw's
// neighbour in 0x4BD5E0's run).
S35_EXPORT void __cdecl BenedictionMote_DrawGlow(void) {
    int x, y;
    {
        const unsigned char* const s = Sc();
        x = S16(s + 0x2E);
        y = S16(s + 0x30);
        SetSW(0, 8);
        SetSW(2, 0x14);
        SetSW(8, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[9])));
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[9])));
        SetSW(0xC, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[9])));
    }
    int angle = 0;
    for (unsigned count = 8; count != 0; --count) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SinSS(angle, 2) + x);
        PutFloat(p + 0xC, CosSS(angle, 2) + y);
        PutFloat(p + 0x28, SinSS(angle, 0) + x);
        PutFloat(p + 0x2C, CosSS(angle, 0) + y);
        angle += 0x200;
        PutFloat(p + 0x18, SinSS(angle, 2) + x);
        PutFloat(p + 0x1C, CosSS(angle, 2) + y);
        PutFloat(p + 0x38, SinSS(angle, 0) + x);
        const int y3 = CosSS(angle, 0) + y;
        p[4] = 1;
        p[5] = 1;
        PutFloat(p + 0x3C, y3);
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = SB(8);
        p[0x25] = SB(0xA);
        p[0x26] = SB(0xC);
        p[0x34] = SB(8);
        p[0x35] = SB(0xA);
        p[0x36] = SB(0xC);
        LinkAt(Sc(), 2, 0x44);
    }
}

// original 0x4F1DE0: the first of the 80 motes of the second pool without bit
// 0, which it sets; its number in al, or 0xFF when none is free.
S35_EXPORT unsigned __cdecl BenedictionMote_Alloc(void) {
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        if ((r[0] & 1) == 0) {
            r[0] = static_cast<unsigned char>(r[0] | 1);
            return n;
        }
    }
    return 0xFF;
}

void BenedictionChild_Inject() {
    BOF3_INJECT(Benediction_Task);
    BOF3_INJECT(Benediction_Start);
    BOF3_INJECT(Benediction_Spawn);
    BOF3_INJECT(Benediction_Wait);
    BOF3_INJECT(Benediction_DrawHalo);
    BOF3_INJECT(BenedictionChild_Task);
    BOF3_INJECT(BenedictionChild_Run);
    BOF3_INJECT(BenedictionChild_Start);
    BOF3_INJECT(BenedictionChild_Tint);
    BOF3_INJECT(BenedictionChild_Brighten);
    BOF3_INJECT(BenedictionChild_Fade);
    BOF3_INJECT(BenedictionChild_End);
    BOF3_INJECT(BenedictionMote_Task);
    BOF3_INJECT(BenedictionMote_Run);
    BOF3_INJECT(BenedictionMote_Launch);
    BOF3_INJECT(BenedictionMote_Spiral);
    BOF3_INJECT(BenedictionMote_Fade);
    BOF3_INJECT(BenedictionMote_DrawGlow);
    BOF3_INJECT(BenedictionMote_Alloc);
}

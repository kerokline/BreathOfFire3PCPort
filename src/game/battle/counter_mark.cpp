// Moved here by tools/move_funcs.py from magic_s08.cpp (docs/source-layout-round.md).
#include "game/battle/counter_mark.h"

#include <cstdint>
#include <cstring>

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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// kFormIndex: now BattlePhases_B89 (symbols.toml). u8: indexes CounterMark_FormIndex
// kTints: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords, 12 bytes a record
// kWardPool: now WardMote_Pool (symbols.toml). WardMote_Pool: 64 records of 0x84
// kTrails: now EvilEyeBeam_Trails (symbols.toml). EvilEyeBeam_Trails: 32 points of 4 bytes a beam
// kMemberOffsets: now CounterMark_MemberOffsets (symbols.toml). CounterMark_MemberOffsets
// kFormOffsets: now CounterMark_FormOffsets (symbols.toml). CounterMark_FormOffsets
constexpr ::bof3::image::Va kFormBytes{0x65A8E8};     // CounterMark_FormIndex

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn2 = void (__cdecl*)(unsigned, unsigned);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_WORD(Fn2, address)(a, b); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The task's position +0x34 / +0x38 / +0x3C from the owner's.
void OwnerPosition() {
    const unsigned char* const o = Owner();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = rv::As<records::SpriteHead>(o)->x;
    ts->z = rv::As<records::SpriteHead>(o)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(o)->height);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A6DF0: the kind-2 task. A two-entry stack table by +1:
// Counter_Start, BattleFx_Finish.
S08_EXPORT void __cdecl Counter_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Counter_Start, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 2, Sc()[1], "Counter_Task");
}

// original 0x4A6E20: the owner's direction and position; +0xB 0, +1 on; one
// mark (kind 1, 0x3E; +1 0) owned by this task and counted in +0xB; sound
// 0x100.
S08_EXPORT void __cdecl Counter_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x3E);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    Inc(self[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A6EC0: the mark's kind-1 task, a jmp through CounterChild_Kinds
// (one entry) by +1.
S08_EXPORT void __cdecl CounterChild_Task(void) { CellCall(::bof3::Img(0x65A834), 1, Sc()[1], "CounterChild_Task"); }

// original 0x4A6EE0: a five-entry stack table by +2 (_Start, _Open, _Grow,
// _Swell, _Fade); then while +0 bit 0 and +2: the disc, the spokes (0, +9)
// and three arcs (+0xB + 8, + 0x10, + 0x18; radius +0xA).
S08_EXPORT void __cdecl CounterMark_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::CounterMark_Start, bof3::addr::CounterMark_Open,
                                                bof3::addr::CounterMark_Grow, bof3::addr::CounterMark_Swell,
                                                bof3::addr::CounterMark_Fade};
    StackCall(kSteps, 5, Sc()[2], "CounterMark_Run");
    const unsigned char* s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::CounterMark_DrawDisc);
    Call2(bof3::addr::CounterMark_DrawSpokes, 0, Sc()[9]);
    for (unsigned k = 8; k <= 0x18; k += 8) {
        s = Sc();
        Call2(bof3::addr::CounterMark_DrawArc, (s[0xB] + k) & 0xFFu, s[0xA]);
    }
}

// original 0x4A6F90: the owner's direction and position, the screen point
// (CounterMark_Place), +0xB, +9, +0xA 0, +2 on.
S08_EXPORT void __cdecl CounterMark_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Call0(bof3::addr::CounterMark_Place);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A7000: +0xB (the spin) up, +0xA up; at 4 +2 on.
S08_EXPORT void __cdecl CounterMark_Open(void) {
    Inc(Sc()[0xB]);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 4) Inc(Sc()[2]);
}

// original 0x4A7030: +0xB up, +9 up by 8; at 0x50 +2 on.
S08_EXPORT void __cdecl CounterMark_Grow(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[9], 8);
    if (Sc()[9] == 0x50) Inc(Sc()[2]);
}

// original 0x4A7060: +0xB up, +9 and +0xA up by 4; at +0xA 0x20 +2 on.
S08_EXPORT void __cdecl CounterMark_Swell(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[9], 4);
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4A70A0: +0xB up; +9 down by 0xC unless 0; +0xA down by 4; at 0
// the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl CounterMark_Fade(void) {
    Inc(Sc()[0xB]);
    unsigned char* const s = Sc();
    if (s[9] != 0) AddB(s[9], 0xF4);
    AddB(Sc()[0xA], 0xFC);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A70F0: the task's screen point (BattleActor_UpdateScreenXY);
// for a party target (0..2) moved by a word pair from a table by direction
// (+8 >> 1) and character: CounterMark_FormOffsets by
// CounterMark_FormIndex[0x904B89] while the member's +0x134 has bit 1, else
// CounterMark_MemberOffsets by the member's +0x89. The x is added for a
// direction of 0 or 3 and taken away otherwise; the y added.
S08_EXPORT void __cdecl CounterMark_Place(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned target = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kTarget)))) & 0xFFu;
    if (target >= 3) return;
    const unsigned char* const member = PartyRecord(target);
    std::uint32_t table;
    unsigned row;
    if (member[0x134] & 2) {
        table = ::bof3::Img(CounterMark_FormOffsets_addr);
        row = Mem(::bof3::Img(kFormBytes.v) + Mem(::bof3::Img(BattlePhases_B89_addr))[0])[0];
    } else {
        table = ::bof3::Img(CounterMark_MemberOffsets_addr);
        row = member[0x89];
    }
    unsigned char* const s = Sc();
    const unsigned facing = s[8];
    const unsigned dx = Word(Mem(table + ((facing >> 1) + row * 2) * 4));
    if (facing == 0 || facing == 3) AddWord(s + 0x2E, dx);
    else AddWord(s + 0x2E, 0u - dx);
    AddWord(s + 0x30, Word(Mem(table + ((s[8] >> 1) + row * 2) * 4 + 2)));
}

// original 0x4A7220: from the screen point (+0x2E / +0x30, kept in the
// scratch): per step of the byte angle `first` (then + 0x10, while below
// `first` + 0x20; angle (step & 0x1F) << 7) three semi-transparent gouraud
// lines (layer 3) - one of `radius` (0xC0 to 1), two of two thirds of it a
// pixel above and below (0x80 to 1). Its one caller passes 0.
S08_EXPORT void __cdecl CounterMark_DrawSpokes(unsigned first, unsigned radius) {
    const unsigned char* const s = Sc();
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned step = first & 0xFF;
    const int end = static_cast<int>(step) + 0x20;
    // The original's counter is a byte: from 0xE0 up it wraps below its end
    // forever, emitting lines until the packet buffer runs out.
    if (step >= 0xE0) bof3::Fatal("CounterMark_DrawSpokes: first angle 0x%X never reaches its end", step);
    const int r = static_cast<int>(radius & 0xFFFF);
    const unsigned two_thirds = static_cast<unsigned>(r / 3 * 2);
    do {
        const unsigned angle = (step & 0x1F) << 7;
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        SetSW(8, angle);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, r) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, r) + SS(2));
        p[4] = 0xC0;
        p[5] = 0xC0;
        p[6] = 0xC0;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
        for (int side = -1; side <= 1; side += 2) {
            p = Gfx_PacketNext;
            if (side < 0) SetSW(4, two_thirds);
            MH_CALL(Gpu_SetLineG2)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            PutFloat(p + 8, SS(0));
            SetSW(8, angle);
            PutFloat(p + 0xC, SS(2) + side);
            v = MH_CALL(Math_Cos)(SS(8));
            PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
            v = MH_CALL(Math_Sin)(SS(8));
            PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2) + side);
            p[4] = 0x80;
            p[5] = 0x80;
            p[6] = 0x80;
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
            MH_CALL(Gfx_CommitPrim)(3, 0x24);
        }
        step = (step + 0x10) & 0xFF;
    } while (static_cast<int>(step) < end);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A7530: from the screen point, four semi-transparent gouraud
// lines (layer 3; 0x80, 0x80, 0x20 to 1) of `radius`, at the word angles
// `first`, + 0x20, + 0x40, + 0x60 (angle (step & 0x7F) << 5).
S08_EXPORT void __cdecl CounterMark_DrawArc(unsigned first, unsigned radius) {
    const unsigned char* const s = Sc();
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const int end = static_cast<int>(first & 0xFF) + 0x80;
    const int r = static_cast<int>(radius & 0xFFFF);
    for (std::uint16_t step = static_cast<std::uint16_t>(first & 0xFF); static_cast<short>(step) < end;
         step = static_cast<std::uint16_t>(step + 0x20)) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        SetSW(8, (step & 0x7Fu) << 5);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, r) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, r) + SS(2));
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x20;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A7690: a disc of sixteen semi-transparent gouraud triangles
// (layer 3) round the screen point, radius Rand & 3 + +0xA / 2; the centre
// 0x60, the rim 1.
S08_EXPORT void __cdecl CounterMark_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const std::uint32_t r = RandCall();
    const unsigned char* const s = Sc();
    SetSW(4, (r & 3) + (s[0xA] >> 1));
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    for (int a = 0; a < 0x2000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = 0x60;
        p[5] = 0x60;
        p[6] = 0x60;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

void CounterMark_Inject() {
    BOF3_INJECT(Counter_Task);
    BOF3_INJECT(Counter_Start);
    BOF3_INJECT(CounterChild_Task);
    BOF3_INJECT(CounterMark_Run);
    BOF3_INJECT(CounterMark_Start);
    BOF3_INJECT(CounterMark_Open);
    BOF3_INJECT(CounterMark_Grow);
    BOF3_INJECT(CounterMark_Swell);
    BOF3_INJECT(CounterMark_Fade);
    BOF3_INJECT(CounterMark_Place);
    BOF3_INJECT(CounterMark_DrawSpokes);
    BOF3_INJECT(CounterMark_DrawArc);
    BOF3_INJECT(CounterMark_DrawDisc);
}

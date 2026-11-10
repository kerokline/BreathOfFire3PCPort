// Moved here by tools/move_funcs.py from magic_s08.cpp (docs/source-layout-round.md).
#include "game/magic/ward_mote.h"

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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
constexpr ::bof3::image::Va kAbility{0x904B80};       // u16: the ability being cast

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddLong(unsigned char* p, std::uint32_t v) {
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + v));
}

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* TintRecord(unsigned index) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + index * 12u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Alloc = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

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

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S08_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four vertex-scratch SVECTORs projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void LinkAtTask(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy, size);
}

int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }

// A pool of 64 records of 0x84 bytes (WardMote_Pool).
void ClearPool(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// Every live record (bit 0 of +0) run as Sprite_Current with its +0x80 as the
// owner, then the task and owner as they were after the phase call.
void WalkPool(std::uint32_t pool, std::uint32_t run) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(run);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

void MoteMove() {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    AddLong(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
    AddLong(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
    Inc(s[9]);
}

struct Matrix {
    short m[10];
    long t[3];
};

}  // namespace

// original 0x4A7830: the kind-2 task. A five-entry stack table by +1
// (Ward_Start, _Tint, _Brighten, _Fade, BattleFx_Finish); then every live
// record of WardMote_Pool run (WardMote_Task).
S08_EXPORT void __cdecl Ward_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Ward_Start, bof3::addr::Ward_Tint, bof3::addr::Ward_Brighten,
                                                 bof3::addr::Ward_Fade, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 5, Sc()[1], "Ward_Task");
    WalkPool(::bof3::Img(WardMote_Pool_addr), bof3::addr::WardMote_Task);
}

// original 0x4A78C0: the pool cleared; the source sprite's direction and
// position; +0xB 0, +9 9, +1 on; eight motes from the pool (WardMote_Alloc;
// none when it answers 0xFF): +0x80 this task, +0xB the index 0..7, +9 the
// delay 1, 9, .., 0x39, counted in +0xB; row 26's first 32 CLUT entries from
// their source as they are; Gfx_ClutStripDirty; sound 0x100.
S08_EXPORT void __cdecl Ward_Start(void) {
    ClearPool(::bof3::Img(WardMote_Pool_addr));
    const unsigned char* const src = Source();
    Sc()[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[0xB] = 0;
    Sc()[9] = 9;
    Inc(Sc()[1]);
    unsigned char index = 0;
    for (unsigned delay = 1; delay < 0x41; delay += 8) {
        const unsigned char got = MH_AT(Alloc, bof3::addr::WardMote_Alloc)();
        if (got != 0xFF) {
            unsigned char* const self = Sc();
            unsigned char* const rec = Mem(::bof3::Img(WardMote_Pool_addr) + got * 0x84u);
            SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
            rec[0xB] = index;
            rec[9] = static_cast<unsigned char>(delay);
            Inc(self[0xB]);
        }
        ++index;
    }
    for (unsigned k = 0x1A00; k < 0x1A20; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A79D0: +9 down; at 0 the source sprite tinted black, its tint
// slot to +0xA, +9 4, +1 on.
S08_EXPORT void __cdecl Ward_Tint(void) {
    unsigned char* const src = Source();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4A7A30: the tint record +0xA brighter by 2 in each channel, +9
// down; at 0 +9 4, +1 on.
S08_EXPORT void __cdecl Ward_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(TintRecord(s[0xA])[c], 2);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4A7AB0: the tint record darker by 2, +9 down; at 0, while two or
// more motes live (+0xB), +9 4 and back to _Brighten; else the tint released,
// the target flashed, and for ability 0x2B (the word 0x904B80) the target's
// buff 1 (MagicFx_ApplyBuff) and a child (kind 1, 0x48; +4 0 when the buff
// took, else 8; +9 1, +0xA 0) counted in +0xB; +1 on.
S08_EXPORT void __cdecl Ward_Fade(void) {
    unsigned char* s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(TintRecord(s[0xA])[c], 0xFE);
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    if (s[0xB] >= 2) {
        s[9] = 4;
        Dec(Sc()[1]);
        return;
    }
    MH_CALL(Tint_Release)(s[0xA]);
    MH_CALL(BattleActor_Flash)(TargetByte());
    if (Word(Mem(::bof3::Img(kAbility.v))) == 0x2B) {
        const unsigned char took = MH_CALL(MagicFx_ApplyBuff)(1, TargetByte());
        const unsigned slot = NewTask(0x48);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[4] = took != 0 ? 0 : 8;
        child[9] = 1;
        child[0xA] = 0;
        Inc(self[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4A7BE0: a mote's task, a jmp through WardMote_TaskTable (one
// entry) by +1.
S08_EXPORT void __cdecl WardMote_Task(void) { CellCall(::bof3::Img(0x65A904), 1, Sc()[1], "WardMote_Task"); }

// original 0x4A7C00: a call through WardMote_Steps (four entries) by +2; then
// while +0 and +2 are set: its matrix, its band, the pop.
S08_EXPORT void __cdecl WardMote_Run(void) {
    CellCall(::bof3::Img(0x65A908), 4, Sc()[2], "WardMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::WardMote_PushMatrix);
    Call0(bof3::addr::WardMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A7C40: +9 (the delay) down; at 0: the owner's direction; the
// offset (0x20000, 0) turned by it from the owner, 0x1000000 above it; the
// step (-0x1000, 0) turned; +0x14 the angle to the owner from the field's
// kind-2 point (Math_Ratan2 of dx, dz); +9 and +0xA 0, +2 on.
S08_EXPORT void __cdecl WardMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->x) +
                                                    static_cast<std::uint32_t>(ts->step_x));
        ts->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->z) +
                                                    static_cast<std::uint32_t>(ts->step_z));
        rv::Store(ts->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->height) + 0x1000000u));
        ts->step_x = static_cast<std::int32_t>(0xFFFFF000u);
        ts->step_z = 0;
    }
    Turn(Sc());
    const unsigned char* const o = Owner();
    const int dz = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->z) - static_cast<std::uint32_t>(Field_Kind2Z));
    const int dx = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->x) - static_cast<std::uint32_t>(Field_Kind2X));
    const int angle = Ratan2(dx, dz);
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A7D50: moved; +0xA up by 2; at 0x10 +2 on.
S08_EXPORT void __cdecl WardMote_Open(void) {
    MoteMove();
    AddB(Sc()[0xA], 2);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4A7DA0: moved; at +9 0x18 +2 on.
S08_EXPORT void __cdecl WardMote_Fly(void) {
    MoteMove();
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4A7DE0: moved; +0xA down by 2; at 0 the owner's +0xB down and
// the record's +0..+4 cleared (the pool slot free).
S08_EXPORT void __cdecl WardMote_End(void) {
    MoteMove();
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    unsigned char* const s = Sc();
    for (unsigned k = 0; k < 5; ++k) s[k] = 0;
}

S08_EXPORT void __cdecl WardMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const short rot[4] = {0x400, static_cast<short>(Word(Mem(::bof3::Img(WardMote_Tilts_addr) + s[8] * 2u))), 0, 0};
    short v[4];
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S08_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4A7F10: a band of eight semi-transparent gouraud quads (tpage
// 0x35, layer 0, linked at the mote) between a ring of radius +9 x 12 and one
// of +9 x 8 on the mote's plane; the inner edge coloured from +0xA - (Rand &
// 7) + 4 times it in each channel for ability 0x2B, else x 4, x 8, x 4 -, the
// outer edge 1.
S08_EXPORT void __cdecl WardMote_Draw(void) {
    DrawMode(0x35);
    LinkAtTask(0, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, s[9] * 8u);
    SetSW(2, s[9] * 12u);
    unsigned last;
    if (Word(Mem(::bof3::Img(kAbility.v))) == 0x2B) {
        std::uint32_t r = RandCall();
        SetSW(6, ((r & 7) + 4) * Sc()[0xA]);
        r = RandCall();
        SetSW(8, ((r & 7) + 4) * Sc()[0xA]);
        r = RandCall();
        last = ((r & 7) + 4) * Sc()[0xA];
    } else {
        SetSW(6, s[0xA] * 4u);
        SetSW(8, s[0xA] * 8u);
        last = s[0xA] * 4u;
    }
    SetSW(0xA, last);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1C, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(8, VW(0));
        SetVW(0xA, VW(2));
        v = MH_CALL(Math_Sin)(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(a);
        {
            const int inner = Mul12(v, SS(2));
            const unsigned x = VW(0x10);
            SetVW(2, static_cast<unsigned>(inner));
            SetVW(0x18, x);
            SetVW(0x1A, VW(0x12));
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = SB(6);
        p[5] = SB(8);
        p[6] = SB(0xA);
        p[0x14] = SB(6);
        p[0x15] = SB(8);
        p[0x16] = SB(0xA);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        LinkAtTask(0, 0x44);
    }
}

// original 0x4A81F0: the first of WardMote_Pool's 64 records with bit 0 of
// +0 clear gets it, its index in al; 0xFF when all are taken.
S08_EXPORT unsigned char __cdecl WardMote_Alloc(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(::bof3::Img(WardMote_Pool_addr) + i * 0x84u);
        if (rec[0] & 1) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void WardMote_Inject() {
    BOF3_INJECT(Ward_Task);
    BOF3_INJECT(Ward_Start);
    BOF3_INJECT(Ward_Tint);
    BOF3_INJECT(Ward_Brighten);
    BOF3_INJECT(Ward_Fade);
    BOF3_INJECT(WardMote_Task);
    BOF3_INJECT(WardMote_Run);
    BOF3_INJECT(WardMote_Launch);
    BOF3_INJECT(WardMote_Open);
    BOF3_INJECT(WardMote_Fly);
    BOF3_INJECT(WardMote_End);
    BOF3_INJECT(WardMote_PushMatrix);
    BOF3_INJECT(WardMote_Draw);
    BOF3_INJECT(WardMote_Alloc);
}

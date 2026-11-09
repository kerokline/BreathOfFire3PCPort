// Moved here by tools/move_funcs.py from effect_1b.cpp, effect_1c.cpp, field_c1.cpp, rest_3e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_17.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/effect/effect_kind_17_callees.h"
#include "game/effect_1b.h"
#include "game/effect_1b_callees.h"
#include "game/effect_1c.h"
#include "game/effect_1c_callees.h"
#include "game/field_c1.h"
#include "game/field_c1_callees.h"
#include "game/fishing_text.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_17::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using U = std::uint32_t;

unsigned char* Sc() { return Sprite_Current; }

// A .data state table read in place: the index a byte of the record, unchecked
// in the original, which jumps through whatever follows the table past its
// length; ours aborts there. While the fuzz runs its entries are recorders.
using Handler = void (__cdecl*)();

Handler StepAt(U table, unsigned index, unsigned count, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past the %u entries of 0x%X (the original jumps through what follows)", who, index,
                    count, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

using move_script::SetLong;

using move_script::SetWord;

U AddressOfFieldC1(const void* p) { return Low32(p); }

U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

short S16(U v) { return static_cast<short>(v & 0xFFFFu); }

// cdq / xor / sub: the absolute value, 0x80000000 staying itself.
std::int32_t Abs32(U v) { return static_cast<std::int32_t>(S32(v) < 0 ? 0u - v : v); }

// The records the handlers write by an index byte the original never checks.
unsigned char* SpriteRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(field_c1::at::kSpriteCount))
        bof3::Fatal("%s: Sprite_Objects index %d - the original writes outside the records", who, i);
    return rv::Bytes(Sprite_Objects) + static_cast<U>(i) * field_c1::at::kRecordStride;
}

unsigned char* ExtraRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(field_c1::at::kExtraCount))
        bof3::Fatal("%s: Sprite_ObjectsExtra index %d - the original writes outside the records", who, i);
    return rv::Bytes(Sprite_ObjectsExtra) + static_cast<U>(i) * field_c1::at::kRecordStride;
}

unsigned char* Member(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(at::kMemberCount))
        bof3::Fatal("%s: ObjTrio index %d - the original writes outside the records", who, i);
    return rv::Bytes(ObjTrio) + static_cast<U>(i) * at::kMemberStride;
}

void Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// A byte of the area block's cell words (AreaMap_Header's word +2 the offset,
// in pairs of words): 0 marks a cell with no floor. The original indexes with
// the signed cell coordinates, unchecked; read in place.
U CellWord(short cx, short cz) {
    const int width = static_cast<int>(AreaMap_Header[0]);
    const int index = width * cz + static_cast<int>(Word(At(::bof3::Img(AreaMap_Header_addr) + 0x2))) * 2 + cx;
    return Word(At(AddressOfFieldC1(AreaMap_Header) + static_cast<U>(index) * 2u));
}

// The object at the effect's point (Sprite_ObjectAt's index): its +0x80 bit 0
// set - Sprite_Objects below 0x1E, Sprite_ObjectsExtra from there (the index
// a signed byte, as the original's movsx).
void MarkObject(unsigned char found, const char* who) {
    const int i = static_cast<signed char>(found);
    unsigned char* const rec = i < 0x1E ? SpriteRec(i, who) : ExtraRec(i - 0x1E, who);
    rec[0x80] = static_cast<unsigned char>(rec[0x80] | 1);
}

// The start both kinds share: two free sprites into +3 and +4 (none: nothing,
// the first given back if only the second fails), each marked in use; for
// each, SharedScratch's word = its index and the op run (EventOp_6x with
// Sprite_SetAnimationAt(0x61, 0xC) for 0x14, EventOp_0x for 0x3C);
// Sprite_Current put back; the first moved by (dx, -dx), the second by (-dx,
// dx), both +0 |= 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1);
// the effect's +9 = 4 and +1 = 1 (Sprite_Current read again after the tints).
template <bool kKind14>
void TwinStart(const char* who) {
    Cur()[3] = SH_CALL(Sprite_FindFree)();
    if (Cur()[3] == 0xFF) return;
    SpriteRec(Cur()[3], who)[0] = 1;
    Cur()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = Cur();
    if (self[4] == 0xFF) {
        SpriteRec(self[3], who)[0] = 0;
        return;
    }
    SpriteRec(self[4], who)[0] = 1;
    const auto* const op = At(kKind14 ? AddressOfFieldC1(EffectKind14_Op) : AddressOfFieldC1(EffectKind3C_Op));
    SharedScratch.w_00 = self[3];
    if (kKind14) {
        SH_CALL(EventOp_6x)(op);
        SH_CALL(Sprite_SetAnimationAt)(0x61, 0xC);
    } else {
        SH_CALL(EventOp_0x)(op);
    }
    SharedScratch.w_00 = self[4];
    if (kKind14) {
        SH_CALL(EventOp_6x)(op);
        SH_CALL(Sprite_SetAnimationAt)(0x61, 0xC);
    } else {
        SH_CALL(EventOp_0x)(op);
    }
    Sprite_Current = self;
    const U d = kKind14 ? 0x1000u : 0xFFFFF000u;
    records::SpriteObject* a = rv::As<records::SpriteObject>(SpriteRec(self[3], who));
    a->x = static_cast<U>(a->x) + d;
    a->z = static_cast<U>(a->z) - d;
    records::SpriteObject* b = rv::As<records::SpriteObject>(SpriteRec(self[4], who));
    b->x = static_cast<U>(b->x) - d;
    b->z = static_cast<U>(b->z) + d;
    a = rv::As<records::SpriteObject>(SpriteRec(self[3], who));
    a->flags = static_cast<unsigned char>(a->flags | 0x20);
    b = rv::As<records::SpriteObject>(SpriteRec(self[4], who));
    b->flags = static_cast<unsigned char>(b->flags | 0x20);
    SpriteRec(self[4], who)[0x5C] = 0;
    SpriteRec(self[3], who)[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(SpriteRec(self[3], who), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(SpriteRec(self[4], who), 0, 0, 0xF, 1);
    rv::As<records::EffectObject>(Cur())->timer = 4;
    rv::As<records::EffectObject>(Cur())->state = 1;
}

unsigned char* S() { return Sprite_Current; }

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

U AddressOfRest3e(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3e.md section 7)",
                    who, state, entries, (unsigned)::bof3::VaOf(AddressOfRest3e(table)));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULRest3e(static_cast<const unsigned char*>(table) + 4 * state)))();
}

}  // namespace

// original 0x46A5E0 (Effect_KindHandlers[0x14]): EffectKind14_States[+1], a tail jump.
extern "C" void __cdecl EffectKind14_Run() {
    StepAt(::bof3::Img(EffectKind14_States_addr), Sc()[1], at::kKind14Count, "EffectKind14_Run")();
}

// original 0x46A600 (EffectKind14_States[0]): the shared start with
// EventOp_6x(EffectKind14_Op) and Sprite_SetAnimationAt(0x61, 0xC) for each,
// the first sprite moved by (+0x1000, -0x1000).
extern "C" void __cdecl EffectKind14_Start(void) { TwinStart<true>("EffectKind14_Start"); }

// original 0x46A7F0 (EffectKind14_States[1]): while +9 is not 0 the leader's
// +0 |= 0x40 and +9 down; at 0 the leader's bit 0x40 cleared, both sprites'
// +0 = 0 and +1 = 2.
extern "C" void __cdecl EffectKind14_Hold(void) {
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->timer != 0) {
        ObjTrio[0].flags = static_cast<unsigned char>(ObjTrio[0].flags | 0x40);
        o->timer = static_cast<unsigned char>(o->timer - 1);
        return;
    }
    ObjTrio[0].flags = static_cast<unsigned char>(ObjTrio[0].flags & 0xBF);
    SpriteRec(rv::Bytes(o)[3], "EffectKind14_Hold")[0] = 0;
    SpriteRec(rv::Bytes(o)[4], "EffectKind14_Hold")[0] = 0;
    o->state = 2;
}

// original 0x46A850 (Effect_KindHandlers[0x36], hidden in ours
// EffectKind32_Arc's recorded extent): +9 counting down first; at 0 a VRAM
// move (Gpu_SetDrawMove at Gfx_PacketNext) of the 16 x 40 rectangle at
// EffectKind36_Frames[+1] (x / 4 toward zero + 0x2C0, y + 0x100) to frame 0's
// (the same sums, as dwords); +1 up, +0x29 0, +9 the new +1, committed at
// slot +0x29 (0x18 bytes); at +1 4, Effect_Release. An index past the four is
// the original's read of what follows the table: ours aborts.
extern "C" void __cdecl EffectKind36_Run(void) {
    unsigned char* s = Cur();
    if (s[9]) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    const unsigned i = s[1];
    if (i >= 4) bof3::Fatal("EffectKind36_Run: +1 is %u, past EffectKind36_Frames' four", i);
    const unsigned char* const frames = reinterpret_cast<const unsigned char*>(EffectKind36_Frames);
    short rect[4];
    rect[0] = static_cast<short>(SW(frames + 4 * i) / 4 + 0x2C0);
    rect[1] = static_cast<short>(Word(frames + 4 * i + 2) + 0x100u);
    rect[2] = 0x10;
    rect[3] = 0x28;
    const U x = static_cast<U>(SW(frames) / 4 + 0x2C0);
    const U y = static_cast<U>(static_cast<int>(SW(frames + 2)) + 0x100);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), x, y);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Cur()[0x29] = 0;
    s = Cur();
    s[9] = s[1];
    SH_CALL(Gfx_CommitPrim)(Cur()[0x29], 0x18);
    if (Cur()[1] == 4) SH_CALL(Effect_Release)();
}

// original 0x46A930 (Effect_KindHandlers[0x3C], hidden in ours
// EffectKind32_Arc's recorded extent): a tail jump through EffectKind3C_States
// by +1 (three entries: FC1's _Start, _Hold, E2D's EffectTwinSprites_Release).
extern "C" void __cdecl EffectKind3C_Run(void) {
    Entry(AddressOf(EffectKind3C_States), Cur()[1], 3, "EffectKind3C_Run")();
}

// original 0x46A950 (EffectKind3C_States[0]): the shared start with
// EventOp_0x(EffectKind3C_Op) for each, the first sprite moved by (-0x1000,
// +0x1000).
extern "C" void __cdecl EffectKind3C_Start(void) { TwinStart<false>("EffectKind3C_Start"); }

// original 0x46AB30 (EffectKind3C_States[1]; also 0x65461C[35]): as
// EffectKind14_Hold with Sprite_Objects[+0xB] in the leader's place.
extern "C" void __cdecl EffectKind3C_Hold(void) {
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->timer != 0) {
        records::SpriteObject* const s = rv::As<records::SpriteObject>(SpriteRec(rv::Bytes(o)[0xB], "EffectKind3C_Hold"));
        s->flags = static_cast<unsigned char>(s->flags | 0x40);
        o->timer = static_cast<unsigned char>(o->timer - 1);
        return;
    }
    records::SpriteObject* const s = rv::As<records::SpriteObject>(SpriteRec(rv::Bytes(o)[0xB], "EffectKind3C_Hold"));
    s->flags = static_cast<unsigned char>(s->flags & 0xBF);
    SpriteRec(rv::Bytes(o)[3], "EffectKind3C_Hold")[0] = 0;
    SpriteRec(rv::Bytes(o)[4], "EffectKind3C_Hold")[0] = 0;
    o->state = 2;
}

// original 0x46ABB0: jmp [EffectKind17_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind17_Run(void) { Dispatch("EffectKind17_Run", EffectKind17_States, EffectKind17_States_count); }

// original 0x46ABD0 (EffectKind17_States[0]; PSX 0x8019C3B0): Sprite_SetAnimationBank(+0xB);
// EffectKind17_TakeCell (+0xB becomes the map byte under it); +0x29 = 6,
// +0x48 = 1, +0x24, +0x2A, +0x5D..+0x5F 0; the dwords +0xC..+0x20 0; +7 and
// +0xA 0; Sprite_SetAnimation(0); +1 up.
extern "C" void __cdecl EffectKind17_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(Cur()[0xB]);
    SH_CALL(EffectKind17_TakeCell)();
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    rv::Bytes(o)[0x29] = 6;
    rv::Bytes(o)[0x48] = 1;
    rv::Bytes(o)[0x24] = 0;
    rv::Bytes(o)[0x2A] = 0;
    o->shade[0] = 0;
    o->shade[1] = 0;
    o->shade[2] = 0;
    for (U off = 0xC; off <= 0x20; off += 4) SetL(rv::Bytes(o) + off, 0);
    rv::Bytes(o)[7] = 0;
    rv::Bytes(o)[0xA] = 0;
    SH_CALL(Sprite_SetAnimation)(0);
    rv::As<records::EffectObject>(Cur())->state = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->state + 1);
}

// original 0x46AC90 (EffectKind17_States[1]; PSX 0x8019C4A8): with Game_Mode 1 only
// Sprite_UpdateScreenSlot. With no push (+0xA 0): the saved map byte +0xB
// put back at the cell (+0x36, +0x3A) when Field_Request is 5; then the
// update. Pushed: EffectKind17_BlockedAhead - blocked, the byte put back, +9
// = 0, +1 = 4; free, the byte put back, the push +0xA = (+0xA + 1) >> 1, the
// speeds +0xC / +0x10 = the direction's unit (0x6696DC[+8], unchecked) << 2,
// each moved away from 0 by 3 * 0x400 * +0xA - 0xC00; +0x14 and +0x20 0; the
// brakes +0x18 / +0x1C -0x400, 0x400 or 0 against each speed's sign;
// Sprite_SetAnimation(1), +9 = 0, +1 up; then the update.
extern "C" void __cdecl EffectKind17_Push(void) {
    if (Game_Mode == 1) {
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (rv::Bytes(o)[0xA] == 0) {
        if (Field_Request == 5) SH_CALL(AreaMap_SetByte)(o->x_whole, o->z_whole, rv::Bytes(o)[0xB]);
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    const unsigned char blocked = SH_CALL(EffectKind17_BlockedAhead)();
    o = rv::As<records::EffectObject>(Cur());
    if (blocked) {
        SH_CALL(AreaMap_SetByte)(o->x_whole, o->z_whole, rv::Bytes(o)[0xB]);
        rv::As<records::EffectObject>(Cur())->timer = 0;
        rv::As<records::EffectObject>(Cur())->state = 4;
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    SH_CALL(AreaMap_SetByte)(o->x_whole, o->z_whole, rv::Bytes(o)[0xB]);
    o = rv::As<records::EffectObject>(Cur());
    rv::Bytes(o)[0xA] = static_cast<unsigned char>((static_cast<int>(rv::Bytes(o)[0xA]) + 1) >> 1);
    const U push = static_cast<U>(rv::Bytes(o)[0xA]) * 3u * 0x400u - 0xC00u;
    o->step_x = L(At(::bof3::Img(MoveCmd_F9Steps_addr) + 8u * rv::Bytes(o)[8])) << 2;
    U v = static_cast<U>(o->step_x);
    if (S32(v) > 0) o->step_x = v + push;
    else if (S32(v) < 0) o->step_x = v - push;
    o->step_z = L(At(::bof3::Img(MoveCmd_F9Steps_addr) + 4 + 8u * rv::Bytes(o)[8])) << 2;
    v = static_cast<U>(o->step_z);
    if (S32(v) > 0) o->step_z = v + push;
    else if (S32(v) < 0) o->step_z = v - push;
    o->work_20 = 0;
    o->step_y = 0;
    v = static_cast<U>(o->step_x);
    o->work_18 = S32(v) > 0 ? 0xFFFFFC00u : S32(v) < 0 ? 0x400u : 0u;
    v = static_cast<U>(o->step_z);
    o->work_1c = S32(v) > 0 ? 0xFFFFFC00u : S32(v) < 0 ? 0x400u : 0u;
    o->work_20 = 0;
    SH_CALL(Sprite_SetAnimation)(1);
    rv::As<records::EffectObject>(Cur())->timer = 0;
    rv::As<records::EffectObject>(Cur())->state = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->state + 1);
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x46AE30 (EffectKind17_States[2]): the slide. +6 = +7 = 0. Along x
// when +0xC is not 0 (else along z with +0x10 / +0x1C / +0x38): the speed
// plus its brake +0x18, back to the old speed and the brake 0 once it is
// below 0x1000 in size; x += the speed; the distance to the leader along that
// axis. Both brakes 0: +6 = 1, +7 = 0. The fall +0x14 += +0x20, the height
// word +0x3E += the word +0x14. With Field_Kind2Hold 0, the distance above
// 0x40000 and the view's kind-2 point at the leader: that point's whole part
// 5 along the push, MoveScript_FAWord 0 or (ground there - MapView_Elevation)
// / 20 when the view is higher, MoveScript_F3Divisor 0x40, +9 = 0x14. The
// ground under the block: below the height the fall -8, else rest on it.
// EffectKind17_Bump found something: done. Else EffectKind17_BlockedAt(x, z)
// (+6 1, +7 2) or EffectKind17_BlockedAhead (+6 1, +7 1). Stopped (+6): by +7
// - 0 on a whole cell (words +0x34 and +0x38 0) the rest (+7, +0xA 0,
// Sprite_SetAnimation(0), +1 = 5, then the tick), else and for 1
// EffectKind17_AlignTarget(0), 2 _AlignTarget(1). Sprite_ScriptTick,
// Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Slide(void) {
    rv::As<records::EffectObject>(Cur())->param = 0;
    Cur()[7] = 0;
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    U dist;
    const U speed_x = static_cast<U>(o->step_x);
    if (speed_x != 0) {
        o->step_x = static_cast<U>(o->work_18) + speed_x;
        o = rv::As<records::EffectObject>(Cur());
        if (Abs32(static_cast<U>(o->step_x)) < 0x1000) {
            o->step_x = speed_x;
            rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_18, 0);
            o = rv::As<records::EffectObject>(Cur());
        }
        o->x = static_cast<U>(o->x) + static_cast<U>(o->step_x);
        o = rv::As<records::EffectObject>(Cur());
        dist = static_cast<U>(o->x) - static_cast<U>(ObjTrio[0].x);
    } else {
        const U speed_z = static_cast<U>(o->step_z);
        o->step_z = static_cast<U>(o->work_1c) + speed_z;
        o = rv::As<records::EffectObject>(Cur());
        if (Abs32(static_cast<U>(o->step_z)) < 0x1000) {
            o->step_z = speed_z;
            rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_1c, 0);
            o = rv::As<records::EffectObject>(Cur());
        }
        o->z = static_cast<U>(o->z) + static_cast<U>(o->step_z);
        o = rv::As<records::EffectObject>(Cur());
        dist = static_cast<U>(o->z) - static_cast<U>(ObjTrio[0].z);
    }
    const std::int32_t far = Abs32(dist);
    if (static_cast<U>(o->work_18) == 0 && static_cast<U>(o->work_1c) == 0) {
        o->param = 1;
        Cur()[7] = 0;
        o = rv::As<records::EffectObject>(Cur());
    }
    o->step_y = static_cast<U>(o->step_y) + static_cast<U>(o->work_20);
    o = rv::As<records::EffectObject>(Cur());
    o->height_whole = o->height_whole + Word(rv::Bytes(o) + 0x14);
    if (Field_Kind2Hold == 0 && far > 0x40000 && static_cast<U>(Field_Kind2X) == static_cast<U>(ObjTrio[0].x) &&
        static_cast<U>(Field_Kind2Z) == static_cast<U>(ObjTrio[0].z)) {
        o = rv::As<records::EffectObject>(Cur());
        if (S32(static_cast<U>(o->step_x)) > 0) SetWord(At(::bof3::Img(at::kKind2XHigh)), Word(At(::bof3::Img(at::kKind2XHigh))) + 5u);
        else if (S32(static_cast<U>(o->step_x)) < 0) SetWord(At(::bof3::Img(at::kKind2XHigh)), Word(At(::bof3::Img(at::kKind2XHigh))) - 5u);
        else if (S32(static_cast<U>(o->step_z)) > 0) SetWord(At(::bof3::Img(at::kKind2ZHigh)), Word(At(::bof3::Img(at::kKind2ZHigh))) + 5u);
        else if (S32(static_cast<U>(o->step_z)) < 0) SetWord(At(::bof3::Img(at::kKind2ZHigh)), Word(At(::bof3::Img(at::kKind2ZHigh))) - 5u);
        const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z)));
        const short view = S16(static_cast<U>(MapView_Elevation));
        MoveScript_FAWord = 0;
        if (view > ground) MoveScript_FAWord = static_cast<unsigned short>((static_cast<int>(ground) - view) / 20);
        MoveScript_F3Divisor = 0x40;
        rv::As<records::EffectObject>(Cur())->timer = 0x14;
    }
    o = rv::As<records::EffectObject>(Cur());
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(o->x, o->z)));
    o = rv::As<records::EffectObject>(Cur());
    if (static_cast<short>(o->height_whole) > ground) {
        o->work_20 = 0xFFFFFFF8u;
    } else {
        o->step_y = 0;
        rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_20, 0);
        rv::Store<unsigned>(rv::As<records::EffectObject>(Cur())->height_whole, static_cast<unsigned short>(ground));
    }
    if (SH_CALL(EffectKind17_Bump)()) return;
    o = rv::As<records::EffectObject>(Cur());
    if (SH_CALL(EffectKind17_BlockedAt)(o->x, o->z)) {
        rv::As<records::EffectObject>(Cur())->param = 1;
        Cur()[7] = 2;
    } else if (SH_CALL(EffectKind17_BlockedAhead)()) {
        rv::As<records::EffectObject>(Cur())->param = 1;
        Cur()[7] = 1;
    }
    o = rv::As<records::EffectObject>(Cur());
    if (o->param != 0) {
        switch (rv::Bytes(o)[7]) {
        case 0:
            if (o->x_frac == 0 && o->z_frac == 0) {
                rv::Bytes(o)[7] = 0;
                Cur()[0xA] = 0;
                SH_CALL(Sprite_SetAnimation)(0);
                rv::As<records::EffectObject>(Cur())->state = 5;
                Tick();
                return;
            }
            SH_CALL(EffectKind17_AlignTarget)(0);
            break;
        case 1: SH_CALL(EffectKind17_AlignTarget)(0); break;
        case 2: SH_CALL(EffectKind17_AlignTarget)(1); break;
        default: break;
        }
    }
    Tick();
}

// original 0x46B100 (EffectKind17_States[3]; PSX 0x8019CBAC): the last stretch to a cell
// edge. The start's x, z kept. With a target +0x18: along x when +0xC is not
// 0 (else z with +0x10 / +0x38) the position plus the speed, held at the
// target (and the target 0) once past it in the speed's direction. The
// ground there: below the height word the fall -8, else the fall and its
// speed 0; the height word += the word +0x14. EffectKind17_Bump found
// something: done. Sunk below the ground: the height the ground at the
// start's point, the fall 0. Target and fall both 0: both speeds within
// 0x4000 and EffectKind17_CellBlocked(the cell) clear - +7 and +0xA 0,
// EffectKind17_TakeCell, Sprite_SetAnimation(0), +1 = 5, the tick; else +1 =
// 4. Sprite_ScriptTick, Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Settle(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    const U start_z = static_cast<U>(o->z), start_x = static_cast<U>(o->x);
    if (static_cast<U>(o->work_18) != 0) {
        if (static_cast<U>(o->step_x) != 0) {
            o->x = static_cast<U>(o->step_x) + start_x;
            o = rv::As<records::EffectObject>(Cur());
            const std::int32_t pos = S32(static_cast<U>(o->x)), target = S32(static_cast<U>(o->work_18));
            if (S32(static_cast<U>(o->step_x)) > 0 ? target < pos : target > pos) {
                o->x = static_cast<U>(target);
                rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_18, 0);
            }
        } else {
            o->z = static_cast<U>(o->step_z) + start_z;
            o = rv::As<records::EffectObject>(Cur());
            const std::int32_t pos = S32(static_cast<U>(o->z)), target = S32(static_cast<U>(o->work_18));
            if (S32(static_cast<U>(o->step_z)) > 0 ? target < pos : target > pos) {
                o->z = static_cast<U>(target);
                rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_18, 0);
            }
        }
        o = rv::As<records::EffectObject>(Cur());
    }
    const int ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(o->x, o->z)));
    o = rv::As<records::EffectObject>(Cur());
    if (static_cast<short>(o->height_whole) > ground) {
        o->work_20 = 0xFFFFFFF8u;
    } else {
        o->work_20 = 0;
        rv::Store<U>(rv::As<records::EffectObject>(Cur())->step_y, 0);
    }
    o = rv::As<records::EffectObject>(Cur());
    o->step_y = static_cast<U>(o->step_y) + static_cast<U>(o->work_20);
    o = rv::As<records::EffectObject>(Cur());
    o->height_whole = o->height_whole + Word(rv::Bytes(o) + 0x14);
    if (SH_CALL(EffectKind17_Bump)()) return;
    o = rv::As<records::EffectObject>(Cur());
    if (static_cast<short>(o->height_whole) < ground) {
        const long back = SH_CALL(AreaMap_Elevation)(S32(start_x), S32(start_z));
        rv::Store<unsigned>(rv::As<records::EffectObject>(Cur())->height_whole, static_cast<U>(back));
        rv::Store<U>(rv::As<records::EffectObject>(Cur())->step_y, 0);
        rv::Store<U>(rv::As<records::EffectObject>(Cur())->work_20, 0);
        o = rv::As<records::EffectObject>(Cur());
    }
    if (static_cast<U>(o->work_18) != 0 || static_cast<U>(o->step_y) != 0) {
        Tick();
        return;
    }
    if (Abs32(static_cast<U>(o->step_x)) > 0x4000 || Abs32(static_cast<U>(o->step_z)) > 0x4000) {
        o->state = 4;
        Tick();
        return;
    }
    const unsigned char blocked = SH_CALL(EffectKind17_CellBlocked)(o->x_whole, o->z_whole);
    o = rv::As<records::EffectObject>(Cur());
    if (blocked) {
        o->state = 4;
        Tick();
        return;
    }
    rv::Bytes(o)[7] = 0;
    Cur()[0xA] = 0;
    SH_CALL(EffectKind17_TakeCell)();
    SH_CALL(Sprite_SetAnimation)(0);
    rv::As<records::EffectObject>(Cur())->state = 5;
    Tick();
}

// original 0x46B2C0 (EffectKind17_States[4]; PSX 0x8019CE30): +0x48 = 2, the scales +0x40 and
// +0x44 up 0x4000; +9 down while not 0 and Field_Kind2Hold is 0. Below a
// scale of 2.0 (+0x40 < 0x20000) a tail jump to Sprite_UpdateScreenSlot; at
// it, with the hold 0 and +9 0, EffectKind17_CameraBack and a tail jump to
// Effect_Release; else nothing.
extern "C" void __cdecl EffectKind17_Grow(void) {
    Cur()[0x48] = 2;
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    o->scale_x = static_cast<U>(o->scale_x) + 0x4000u;
    o = rv::As<records::EffectObject>(Cur());
    o->scale_y = static_cast<U>(o->scale_y) + 0x4000u;
    o = rv::As<records::EffectObject>(Cur());
    if (o->timer != 0 && Field_Kind2Hold == 0) {
        o->timer = static_cast<unsigned char>(o->timer - 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    if (S32(static_cast<U>(o->scale_x)) < 0x20000) {
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    if (Field_Kind2Hold != 0 || o->timer != 0) return;
    SH_CALL(EffectKind17_CameraBack)();
    SH_CALL(Effect_Release)();
}

// original 0x46B340 (EffectKind17_States[5]; PSX 0x8019CF04): +9 down while not 0; with the
// hold 0 and +9 0, +1 = 1 and EffectKind17_CameraBack. Sprite_ScriptTick,
// tail Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Rest(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (o->timer != 0) {
        o->timer = static_cast<unsigned char>(o->timer - 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    if (Field_Kind2Hold == 0 && o->timer == 0) {
        o->state = 1;
        SH_CALL(EffectKind17_CameraBack)();
    }
    Tick();
}

// original 0x46B380 (E8 from _Start and _Settle; PSX 0x8019CF80): +0xB = AreaMap_ByteAt(the
// cell +0x36, +0x3A), then AreaMap_SetByte(the cell, 0x10) - the block's
// cell marked, what was there kept.
extern "C" void __cdecl EffectKind17_TakeCell(void) {
    unsigned char* o = Cur();
    const unsigned char under = SH_CALL(AreaMap_ByteAt)(S16(rv::As<records::EffectObject>(o)->x_whole), S16(rv::As<records::EffectObject>(o)->z_whole));
    Cur()[0xB] = under;
    o = Cur();
    SH_CALL(AreaMap_SetByte)(rv::As<records::EffectObject>(o)->x_whole, rv::As<records::EffectObject>(o)->z_whole, 0x10);
}

// original 0x46B3C0 (E8 from _Grow and _Rest; PSX 0x8019CFDC): the view's kind-2 point back
// on the leader (ObjTrio +0x34 / +0x38 into Field_Kind2X / Z),
// MoveScript_FAWord negated, MoveScript_F3Divisor 0x40, Field_Kind2Hold 1.
extern "C" void __cdecl EffectKind17_CameraBack(void) {
    const unsigned short fa = MoveScript_FAWord;
    Field_Kind2X = ObjTrio[0].x;
    MoveScript_FAWord = static_cast<unsigned short>(0u - fa);
    Field_Kind2Z = ObjTrio[0].z;
    MoveScript_F3Divisor = 0x40;
    Field_Kind2Hold = 1;
}

// original 0x46B400 (E8 from _Slide and _Settle; PSX 0x8019D030): Sprite_ObjectAt(+0x34,
// +0x38, 0) found one - its +0x80 bit 0 set - or Party_MemberAt found a
// member other than the leader - its +2 = 1 (the leader, 0, nothing set):
// +1 = 4, Sprite_ScriptTick, Sprite_UpdateScreenSlot, al 1. Neither: al 0.
extern "C" unsigned char __cdecl EffectKind17_Bump(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    const unsigned char found = SH_CALL(Sprite_ObjectAt)(o->x, o->z, 0);
    if (found != 0xFF) {
        MarkObject(found, "EffectKind17_Bump");
    } else {
        o = rv::As<records::EffectObject>(Cur());
        const unsigned char member = SH_CALL(Party_MemberAt)(o->x, o->z, 0);
        if (member == 0xFF) return 0;
        if (member != 0) Member(static_cast<signed char>(member), "EffectKind17_Bump")[2] = 1;
    }
    rv::As<records::EffectObject>(Cur())->state = 4;
    Tick();
    return 1;
}

// original 0x46B4B0 (E8 from _Slide, the byte 0 or 1): off a whole cell in x
// (the word +0x34 not 0): moving up (+0xC > 0) the target +0x18 the next
// cell's edge - x + 0x10000 rounded down, or x rounded down for a flag -,
// moving down x rounded down; +1 = 3 (also with no speed). Else the same
// along z with +0x10 / +0x38; on a whole cell both ways, nothing.
extern "C" void __cdecl EffectKind17_AlignTarget(unsigned flag) {
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    U pos_off, speed_off;
    if (o->x_frac != 0) {
        pos_off = 0x34;
        speed_off = 0xC;
    } else if (o->z_frac != 0) {
        pos_off = 0x38;
        speed_off = 0x10;
    } else {
        return;
    }
    const std::int32_t speed = S32(L(rv::Bytes(o) + speed_off));
    if (speed > 0) {
        const U pos = L(rv::Bytes(o) + pos_off);
        rv::Store<U>(o->work_18, ((flag & 0xFF) == 0 ? pos + 0x10000u : pos) & 0xFFFF0000u);
        rv::As<records::EffectObject>(Cur())->state = 3;
        return;
    }
    if (speed < 0) {
        rv::Store<U>(o->work_18, L(rv::Bytes(o) + pos_off) & 0xFFFF0000u);
        rv::As<records::EffectObject>(Cur())->state = 3;
        return;
    }
    o->state = 3;
}

// original 0x46B580 (E8 from _Push and _Slide): EffectKind17_BlockedAt two
// steps ahead - +0x34 / +0x38 plus twice Field_DirectionSteps[+8] (the byte
// unchecked, read in place); its al.
extern "C" unsigned char __cdecl EffectKind17_BlockedAhead(void) {
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    const U d = rv::Bytes(o)[8];
    const U x = static_cast<U>(o->x), z = static_cast<U>(o->z);
    const U dx = L(At(AddressOfFieldC1(Field_DirectionSteps) + 8u * d));
    const U dz = L(At(AddressOfFieldC1(Field_DirectionSteps) + 4 + 8u * d));
    return SH_CALL(EffectKind17_BlockedAt)(S32(x + dx * 2u), S32(z + dz * 2u));
}

// original 0x46B580's callee 0x46B5C0 (E8 from _Slide and _BlockedAhead):
// whether the block may not stand at (x, z), 16.16. Its own cell blocked
// (EffectKind17_CellBlocked): 1. Moving in x (+0xC not 0): on a whole cell
// (x's low word 0) 0; else the slope at the cell's middle in x
// (AreaMap_Slope(cell x.8000, z, +8)), and where it slopes (SharedScratch) the
// ground there above the height word +0x3E: 1; moving up in x, the next cell
// in x blocked: 1; else 0. Moving in z the same with z. The original reads
// its arguments' high words through its own frame at +2, so the cells passed
// to _CellBlocked carry the next word above - _CellBlocked reads the low
// words only.
extern "C" unsigned char __cdecl EffectKind17_BlockedAt(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U cell_x = ux >> 16, cell_z = uz >> 16;
    if (SH_CALL(EffectKind17_CellBlocked)(cell_x, cell_z)) return 1;
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (static_cast<U>(o->step_x) != 0) {
        if ((ux & 0xFFFF) == 0) return 0;
        const U mid = (cell_x << 16) | 0x8000u;
        SH_CALL(AreaMap_Slope)(S32(mid), z, rv::Bytes(o)[8]);
        if (SharedScratch.b_00 != 0) {
            const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(S32(mid), z)));
            if (static_cast<short>(rv::As<records::EffectObject>(Cur())->height_whole) < ground) return 1;
        }
        if (S32(static_cast<U>(rv::As<records::EffectObject>(Cur())->step_x)) <= 0) return 0;
        return SH_CALL(EffectKind17_CellBlocked)(cell_x + 1, cell_z) ? 1 : 0;
    }
    if ((uz & 0xFFFF) == 0) return 0;
    const U mid = (cell_z << 16) | 0x8000u;
    SH_CALL(AreaMap_Slope)(x, S32(mid), rv::Bytes(o)[8]);
    if (SharedScratch.b_00 != 0) {
        const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(x, S32(mid))));
        if (static_cast<short>(rv::As<records::EffectObject>(Cur())->height_whole) < ground) return 1;
    }
    if (S32(static_cast<U>(rv::As<records::EffectObject>(Cur())->step_z)) <= 0) return 0;
    return SH_CALL(EffectKind17_CellBlocked)(cell_x, cell_z + 1) ? 1 : 0;
}

// original 0x46B6D0 (E8 from _Settle and _BlockedAt): whether cell (x, z)
// (signed words) takes no block: no floor word there (the area block's cell
// word 0): 1. Else by the ground at the cell's corner and its map byte: the
// height word +0x3E at most 0x100 above the ground on a byte 0xFx: 1; level
// with the ground on a byte 0x10, 0x2x or 0xAx: 1; below it: 1; else 0.
extern "C" unsigned char __cdecl EffectKind17_CellBlocked(unsigned cell_x, unsigned cell_z) {
    const short cx = S16(cell_x), cz = S16(cell_z);
    if (CellWord(cx, cz) == 0) return 1;
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(S32(static_cast<U>(cx) << 16), S32(static_cast<U>(cz) << 16))));
    const unsigned char b = SH_CALL(AreaMap_ByteAt)(cx, cz);
    const short height = static_cast<short>(rv::As<records::EffectObject>(Cur())->height_whole);
    if (height - ground <= 0x100 && (b & 0xF0) == 0xF0) return 1;
    if (height == ground) return b == 0x10 || (b & 0xF0) == 0x20 || (b & 0xF0) == 0xA0 ? 1 : 0;
    return height < ground ? 1 : 0;
}

void EffectKind17_Inject() {
    BOF3_INJECT(EffectKind14_Run);
    BOF3_INJECT(EffectKind14_Start);
    BOF3_INJECT(EffectKind14_Hold);
    BOF3_INJECT(EffectKind36_Run);
    BOF3_INJECT(EffectKind3C_Run);
    BOF3_INJECT(EffectKind3C_Start);
    BOF3_INJECT(EffectKind3C_Hold);
    BOF3_INJECT(EffectKind17_Run);
    BOF3_INJECT(EffectKind17_Start);
    BOF3_INJECT(EffectKind17_Push);
    BOF3_INJECT(EffectKind17_Slide);
    BOF3_INJECT(EffectKind17_Settle);
    BOF3_INJECT(EffectKind17_Grow);
    BOF3_INJECT(EffectKind17_Rest);
    BOF3_INJECT(EffectKind17_TakeCell);
    BOF3_INJECT(EffectKind17_CameraBack);
    BOF3_INJECT(EffectKind17_Bump);
    BOF3_INJECT(EffectKind17_AlignTarget);
    BOF3_INJECT(EffectKind17_BlockedAhead);
    BOF3_INJECT(EffectKind17_BlockedAt);
    BOF3_INJECT(EffectKind17_CellBlocked);
}

// Moved here by tools/move_funcs.py from effect_3c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_6f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3c.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

U Shards() { return AddressOf(EffectKind30_Shards); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; sub eax, edx; sar eax, 1: a signed halving toward zero.
U Half(U v) { return Sar(v + (S32(v) < 0 ? 1u : 0u), 1); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

const unsigned char* Story() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(std::int32_t v, void* o) {
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(v) : "st", "memory");
}

// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword
// (eax), which is all the callers keep.
// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fld dword [a]; call _ftol`
U FldFtol(const void* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3c.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 (or +2) up, Sprite_Current read afresh.
void Step(unsigned byte) {
    unsigned char* const s = S();
    s[byte] = static_cast<unsigned char>(s[byte] + 1);
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// Kind 0x6F's colour row, by Sprite_Current +6 (read afresh each time): one row
// of three bytes; a +6 past it reads into the state table after it.
unsigned char Kind6FColour(unsigned byte, const char* who) {
    const unsigned k = S()[6];
    if (k >= at::kKind6FColourCount)
        bof3::Fatal("%s: +6 is %u, past kind 0x6F's one colour row at 0x%X - the original reads EffectKind6F_States' "
                    "bytes as a colour (docs/effect_3c.md section 6)",
                    who, k, at::kKind6FColour.v);
    return At(::bof3::Img(at::kKind6FColour.v) + 3 * k + byte)[0];
}

// ObjTrio record m (Party_MemberAt's answer); past the three, ours aborts.
unsigned char* Member(U m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: Party_MemberAt answered %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_3c.md section 6)",
                    who, (unsigned)m);
    return rv::Bytes(ObjTrio) + at::kObjTrioStride * m;
}

// One of the line's four steps: its top at the ground (+0x3E) + 0x100 and +
// 0x200 (+0x20, copied to +0x14), each drawn from (+0xC..) to (+0x18..), then
// BareRet with the two points (a bare ret: nothing).
void Kind6FSegments() {
    for (U top = 0x100; top <= 0x200; top += 0x100) {
        unsigned char* s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->height_whole)) + static_cast<std::int32_t>(top)) << 16);
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20));
        s = S();
        SH_CALL(EffectKind6F_DrawSegment)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    }
    SH_CALL(BareRet)();
}

}  // namespace

// original 0x4845F0 (Effect_KindHandlers[0x6F], hidden in 0x4844C0):
// jmp [EffectKind6F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6F_Run(void) {
    Dispatch("EffectKind6F_Run", AddressOf(EffectKind6F_States), EffectKind6F_States_count, 1);
}

// original 0x484610 (state 0): Cond_ByteFD 2 with story flag 0x5C clear: +1 =
// 1; else Cond_ByteFD (read again) 3 with flag 0x5D set: +1 = 2; else +1 = 3.
extern "C" void __cdecl EffectKind6F_Start(void) {
    if (Cond_ByteFD == 2 && SH_CALL(Flags_Test)(Story(), 0x5C) == 0) {
        S()[1] = 1;
        return;
    }
    if (Cond_ByteFD == 3 && SH_CALL(Flags_Test)(Story(), 0x5D) != 0) {
        S()[1] = 2;
        return;
    }
    S()[1] = 3;
}

// original 0x484670 (state 1): +6 0; the line along z at x 0x5C0000 (+0xC and
// +0x18), from z 0x98000 (+0x10); its ground AreaMap_Elevation(+0xC, (+0x1C +
// +0x10) / 2) to +0x3E (s16) - +0x1C not yet written, the record's own -;
// EffectKind6F_PushParty(0); then two steps of 0x20000: +0x1C = +0x10 +
// 0x20000, the record's point +0x34 / +0x38 the step's middle, the segments
// (Kind6FSegments), +0x10 = +0x1C, +0x1C += 0x20000. Story flag 0x5C set:
// sound 0x206, +1 = 3.
extern "C" void __cdecl EffectKind6F_AlongZ(void) {
    S()[6] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, 0x5C0000);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x5C0000);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0x98000);
    unsigned char* s = S();
    const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(rv::As<records::EffectWork>(s)->work_0c), static_cast<long>(Half(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + rv::As<records::EffectWork>(s)->work_10)));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(e));
    SH_CALL(EffectKind6F_PushParty)(0);
    for (int i = 2; i != 0; --i) {
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, rv::As<records::EffectWork>(s)->work_10 + 0x20000u);
        s = S();
        rv::As<records::EffectObject>(s)->x = rv::As<records::EffectWork>(s)->work_0c;
        s = S();
        rv::As<records::EffectObject>(s)->z = Half(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + rv::As<records::EffectWork>(s)->work_10);
        Kind6FSegments();
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c));
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x20000u);
    }
    if (SH_CALL(Flags_Test)(Story(), 0x5C) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 3;
    }
}

// original 0x4847D0 (state 2): the same along x at z 0x440000 (+0x1C and
// +0x10), from x 0x3D8000 (+0xC); the ground AreaMap_Elevation((+0x18 + +0xC)
// / 2, +0x10); EffectKind6F_PushParty(1); the steps on +0x18 / +0xC. Story flag
// 0x5D clear: sound 0x206, +1 = 3.
extern "C" void __cdecl EffectKind6F_AlongX(void) {
    S()[6] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x3D8000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, 0x440000);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0x440000);
    unsigned char* s = S();
    const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(Half(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + rv::As<records::EffectWork>(s)->work_0c)), static_cast<long>(rv::As<records::EffectWork>(s)->work_10));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(e));
    SH_CALL(EffectKind6F_PushParty)(1);
    for (int i = 2; i != 0; --i) {
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, rv::As<records::EffectWork>(s)->work_0c + 0x20000u);
        s = S();
        rv::As<records::EffectObject>(s)->x = Half(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + rv::As<records::EffectWork>(s)->work_0c);
        s = S();
        rv::As<records::EffectObject>(s)->z = rv::As<records::EffectWork>(s)->work_10;
        Kind6FSegments();
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18));
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 0x20000u);
    }
    if (SH_CALL(Flags_Test)(Story(), 0x5D) == 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 3;
    }
}

// original 0x484930 (state 3): Cond_ByteFD 2 with flag 0x5C clear, then
// Cond_ByteFD 3 with flag 0x5D set (each tested): sound 0x206, +1 = 0.
extern "C" void __cdecl EffectKind6F_Watch(void) {
    if (Cond_ByteFD == 2 && SH_CALL(Flags_Test)(Story(), 0x5C) == 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 0;
    }
    if (Cond_ByteFD == 3 && SH_CALL(Flags_Test)(Story(), 0x5D) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 0;
    }
}

// original 0x4849A0 (which, a byte): 0 - the four cells of kKind6FCellsZ along
// x 0x5C: the record's point +0x34 / +0x38 put at (0x5C0000, cell << 16) and
// Party_MemberAt(+0x34, +0x38, 0); a member there, with Field_ScriptFlags2 &
// 0x1400 clear and Field_Request 0, faces 7 (the record's x at or left of the
// member's) or 3 (ObjTrio +8) and Member_SetState2_8(member, 2). 1 - the
// cells of kKind6FCellsX along z 0x44 (+0x34 cell << 16, +0x38 0x440000), the
// facing 1 or 5 by z. Anything else: nothing. The original keeps each answer
// in its argument's low byte and hands Member_SetState2_8 the whole slot.
extern "C" void __cdecl EffectKind6F_PushParty(unsigned which) {
    U slot = which;
    const unsigned sel = which & 0xFFu;
    if (sel > 1) return;
    const U cells = sel == 0 ? ::bof3::Img(at::kKind6FCellsZ.v) : ::bof3::Img(at::kKind6FCellsX.v);
    for (unsigned i = 0; i < 4; ++i) {
        const U cell = At(cells + i)[0];
        if (sel == 0) {
            rv::Current<records::EffectObject>()->x = 0x5C0000;
            rv::Store<U>(rv::Current<records::EffectObject>()->z, cell << 16);
        } else {
            rv::Store<U>(rv::Current<records::EffectObject>()->x, cell << 16);
            rv::Current<records::EffectObject>()->z = 0x440000;
        }
        unsigned char* const s = S();
        auto* const so = rv::As<records::EffectObject>(s);
        const unsigned char m = SH_CALL(Party_MemberAt)(so->x, so->z, 0);
        slot = (slot & 0xFFFFFF00u) | m;
        if (m == 0xFF) continue;
        if ((Field_ScriptFlags2 & 0x1400u) != 0) continue;
        if (Field_Request != 0) continue;
        unsigned char* const rec = Member(slot & 0xFFu, "EffectKind6F_PushParty");
        auto* const reco = rv::As<records::PartyMember>(rec);
        if (sel == 0)
            rec[8] = S32(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) - static_cast<std::uint32_t>(reco->x)) <= 0 ? 7 : 3;
        else
            rec[8] = S32(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) - static_cast<std::uint32_t>(reco->z)) <= 0 ? 1 : 5;
        SH_CALL(Member_SetState2_8)(slot, 2);
    }
}

// original 0x484B10 (a, b: two points of three dwords): a draw mode (page
// (0x3C0, 0), dtd 1) linked at the record's point (+0x34, +0x38) with dy 1;
// EffectGte_LoadMapCamera; a LINE_F2 at the cursor, opaque, from a to b
// projected, kind 0x6F's colour, linked (.., 1, 0x20); the screen angle of the
// line Math_Ratan2(dy, dx) (each through _ftol, then a float); the two ends'
// sizes EffectGte_ProjectSize(end, {0x40, 0}, out) - out's dword plus
// Frame_Counter & 1 (read after each); EffectKind6F_DrawRibbon(the four screen
// words through _ftol, the sizes, the angle + 0x400 and + 0xC00); a closing
// draw mode (dtd 0) linked at the point.
extern "C" void __cdecl EffectKind6F_DrawSegment(const long* a, const long* b) {
    DrawMode(1, 0x3C0, 0, 1);
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    SH_CALL(EffectGte_ProjectPoint)(a, reinterpret_cast<float*>(prim + 8));
    SH_CALL(EffectGte_ProjectPoint)(b, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = Kind6FColour(0, "EffectKind6F_DrawSegment");
    prim[5] = Kind6FColour(1, "EffectKind6F_DrawSegment");
    prim[6] = Kind6FColour(2, "EffectKind6F_DrawSegment");
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x20);
    const U dx = FldSubFtol(prim + 0x14, prim + 8);
    float fx, fy;
    Fild(S32(dx), &fx);
    const U dy = FldSubFtol(prim + 0x18, prim + 0xC);
    Fild(S32(dy), &fy);
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(fy, fx));
    short size[2] = {0x40, 0};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(a, size, out);
    U wa;
    std::memcpy(&wa, out, 4);
    wa += Frame_Counter & 1u;
    SH_CALL(EffectGte_ProjectSize)(b, size, out);
    U wb;
    std::memcpy(&wb, out, 4);
    wb += Frame_Counter & 1u;
    const U by = FldFtol(prim + 0x18);
    const U bx = FldFtol(prim + 0x14);
    const U ay = FldFtol(prim + 0xC);
    const U ax = FldFtol(prim + 8);
    SH_CALL(EffectKind6F_DrawRibbon)(S32(ax), S32(ay), S32(wa), S32(angle + 0x400u), S32(bx), S32(by), S32(wb),
                                     S32(angle + 0xC00u));
    DrawMode(1, 0x3C0, 0, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
}

// original 0x484D00 (ax, ay, wa, ta, bx, by, wb, tb - every one read as its low
// s16, the angles masked to 16 bits): two POLY_G4s at the cursor (read once),
// semi-transparent, sharing the edge (ax, ay)-(bx, by) in kind 0x6F's colour;
// the far corners black at (Math_Cos, Math_Sin) * radius >> 12 off the ends -
// the first quad at ta round a (radius wa) and tb + 0x800 round b (wb), the
// second (a copy of the first's 0x44 bytes, linked tag and all) at ta + 0x800
// and tb. Each linked at the record's point (.., 1, 0x44); the second is the
// cursor read at entry + 0x44, wherever the link left the cursor.
extern "C" void __cdecl EffectKind6F_DrawRibbon(int ax, int ay, int wa, int ta, int bx, int by, int wb, int tb) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(q);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    const std::int32_t x0 = static_cast<short>(ax), y0 = static_cast<short>(ay);
    const std::int32_t x1 = static_cast<short>(bx), y1 = static_cast<short>(by);
    Fild(x0, q + 8);
    Fild(y0, q + 0xC);
    Fild(x1, q + 0x18);
    Fild(y1, q + 0x1C);
    const U t1 = static_cast<U>(ta) & 0xFFFFu;
    const std::int32_t r1 = static_cast<short>(wa);
    const std::int32_t r2 = static_cast<short>(wb);
    auto corner = [](int trig, std::int32_t radius, std::int32_t base) {
        return S32(Sar(Mul(static_cast<U>(trig), radius), 12) + static_cast<U>(base));
    };
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t1)), r1, x0), q + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t1)), r1, y0), q + 0x2C);
    const U t2 = static_cast<U>(tb) & 0xFFFFu;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2 + 0x800u)), r2, x1), q + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2 + 0x800u)), r2, y1), q + 0x3C);
    q[4] = Kind6FColour(0, "EffectKind6F_DrawRibbon");
    q[5] = Kind6FColour(1, "EffectKind6F_DrawRibbon");
    q[6] = Kind6FColour(2, "EffectKind6F_DrawRibbon");
    q[0x14] = Kind6FColour(0, "EffectKind6F_DrawRibbon");
    q[0x15] = Kind6FColour(1, "EffectKind6F_DrawRibbon");
    q[0x16] = Kind6FColour(2, "EffectKind6F_DrawRibbon");
    q[0x24] = q[0x25] = q[0x26] = 0;
    q[0x34] = q[0x35] = q[0x36] = 0;
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x44);
    unsigned char* const q2 = q + 0x44;
    std::memcpy(q2, q, 0x44);
    const U t3 = t1 + 0x800u;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t3)), r1, x0), q2 + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t3)), r1, y0), q2 + 0x2C);
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2)), r2, x1), q2 + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2)), r2, y1), q2 + 0x3C);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x44);
}

// original 0x484F50 (Effect_KindHandlers[0x72], hidden in 0x484D00):
// jmp [EffectKind72_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind72_Run(void) {
    Dispatch("EffectKind72_Run", AddressOf(EffectKind72_States), EffectKind72_States_count, 1);
}

// original 0x484F70 (state 0; EffectKind1F_Start's twin): the 32 debris
// records of 0x2C at EffectKind30_Shards set up (EffectDebris_InitOne); +9 0,
// +1 up.
extern "C" void __cdecl EffectKind72_Start(void) {
    for (unsigned i = 0; i < at::kDebrisCount; ++i) SH_CALL(EffectDebris_InitOne)(At(Shards() + at::kDebrisStride * i));
    S()[9] = 0;
    Step(1);
}

// original 0x484FA0 (state 1; EffectKind1F_Debris's twin): a draw mode (page
// (0x380, 0x100), dtd 0) committed (Gfx_CommitPrim(1, 0xC));
// EffectGte_LoadMapCamera; each of the 32 debris drawn (EffectDebris_Draw),
// its angle +0x24 up 0x10, its shade +0x2A up 0x20 while the record's +9
// (Sprite_Current read after the draw) is below 4, else down 2; the last
// Sprite_Current read's +9 up 1; +9 (read again) above 0x44: +1 up.
extern "C" void __cdecl EffectKind72_Debris(void) {
    DrawMode(1, 0x380, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* last = nullptr;
    for (unsigned i = 0; i < at::kDebrisCount; ++i) {
        unsigned char* const d = At(Shards() + at::kDebrisStride * i);
        SH_CALL(EffectDebris_Draw)(d);
        last = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectShard>(d)->angle[2], rv::As<records::EffectShard>(d)->angle[2] + 0x10u);
        rv::Store<std::uint32_t>(rv::As<records::EffectShard>(d)->shade, rv::As<records::EffectShard>(d)->shade + (last[9] < 4 ? 0x20u : 0xFFFEu));
    }
    last[9] = static_cast<unsigned char>(last[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 0x44) s[1] = static_cast<unsigned char>(s[1] + 1);
}

void EffectKind6f_Inject() {
    BOF3_INJECT(EffectKind6F_Run);
    BOF3_INJECT(EffectKind6F_Start);
    BOF3_INJECT(EffectKind6F_AlongZ);
    BOF3_INJECT(EffectKind6F_AlongX);
    BOF3_INJECT(EffectKind6F_Watch);
    BOF3_INJECT(EffectKind6F_PushParty);
    BOF3_INJECT(EffectKind6F_DrawSegment);
    BOF3_INJECT(EffectKind6F_DrawRibbon);
    BOF3_INJECT(EffectKind72_Run);
    BOF3_INJECT(EffectKind72_Start);
    BOF3_INJECT(EffectKind72_Debris);
}

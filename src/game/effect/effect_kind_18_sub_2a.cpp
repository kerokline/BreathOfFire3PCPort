// Moved here by tools/move_funcs.py from effect_5f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_2a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_5f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

U W(U a) { return Word(At(a)); }

U B(U a) { return At(a)[0]; }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }

U AddressOf(const void* p) { return Low32(p); }

// cdq; xor; sub: |v| in 32 bits (0x80000000 stays itself, below every bound)
std::int32_t Abs(U v) {
    const U m = static_cast<U>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ m) - m);
}

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: half the height, toward zero
U Half(long e) { return static_cast<U>(S16(static_cast<U>(e)) / 2); }

unsigned char* Vertex(U a) { return At(a); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next sub-kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5f.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// A placement index the start states read from +0x36 (movsx: a signed word),
// checked against its table's records.
U Place(const char* who, U table, unsigned count) {
    const std::int32_t i = S16(rv::As<records::EffectObject>(S())->x_whole);
    if (i < 0 || i >= static_cast<std::int32_t>(count))
        bof3::Fatal("%s: +0x36 is %d, outside the %u records of 0x%X - the original reads what lies beside them "
                    "(docs/effect_5f.md section 6)",
                    who, (int)i, count, (unsigned)table);
    return static_cast<U>(i);
}

// The leader against the record's cell (+0x36 x, +0x3A z, s16 cells; the
// leader's point 16.16). Along the axis +8 picks (z when set), the leader's
// distance from the cell's far edge's middle ((c + 1) << 16 | 0x8000) is at
// most `edge`; across it, the leader is within `cross` of the cell's or the
// next cell's start. The gate's test is (0x8000, 0x10000); "two cells away"
// is its negation at (0x20000, 0x20000).
bool Within(const unsigned char* s, bool z_axis, std::int32_t edge, std::int32_t cross) {
    const U lx = static_cast<std::uint32_t>(ObjTrio[0].x), lz = static_cast<std::uint32_t>(ObjTrio[0].z);
    const U x = static_cast<U>(S16(Word(s + 0x36))), z = static_cast<U>(S16(Word(s + 0x3A)));
    U along, across;
    if (z_axis) {
        along = lz - (((z + 1) << 16) | 0x8000u);
        across = lx - (x << 16);
    } else {
        along = lx - (((x + 1) << 16) | 0x8000u);
        across = lz - (z << 16);
    }
    return Abs(along) <= edge && (Abs(across) <= cross || Abs(across - 0x10000u) <= cross);
}

bool Near(const unsigned char* s, bool z_axis) { return Within(s, z_axis, 0x8000, 0x10000); }

// The leader within the gate: Sound_PlayEffect(0x200) unless Field_Request,
// then +2 up (Sprite_Current read again after the sound).
void StepIfNear(bool z_axis) {
    unsigned char* s = S();
    if (!Near(s, z_axis)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(0x200);
        s = S();
    }
    ++s[2];
}

// +2 back to 1 with Sound_PlayEffect(0x201) unless Field_Request.
void Closed() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    S()[2] = 1;
}

// The quad's projection, its depths: the four vertices of Prim_VertexScratch
// into the packet's four screen points (the original pushes a tenth word, a
// local, which Gte_RotTransPers4 does not read).
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8)),
                               reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

long Elevation(U x, U z) { return SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)); }

U GroundOf(U word) { return static_cast<U>(S16(word) + 0x4000) << 9; }

// The same for the lifted panels (sub-kinds 0x29 and 0x2A): every height less
// half the ground's and less +0x3E (Sprite_Current read after each call), v0
// and v1 0x180 lower.
void GroundLifted() {
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    U gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v0)->y)), gx = GroundOf(Word(v0));
    long e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole - 0x180u);
    e = Elevation(gx, gz);
    gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v1)->y));
    gx = GroundOf(Word(v1));
    Prim_VertexScratch[2].z = 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole;
    e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole - 0x180u);
    e = Elevation(gx, gz);
    Prim_VertexScratch[3].z = 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole;
}

// Gpu_SetDrawMode(cursor, 0, 0, 0x95, 0): every draw here opens with it.
void DrawMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }

// The next packet as a shaded POLY_FT4.
unsigned char* Quad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}

void LinkAtRecord(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

// The panel's four corners on the axis +8 does not pick: c = (w << 7) - 0x3FC0
// of the record's other cell word, into the four vertices' word `off`.
void Across(unsigned off, U c) {
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr)) + off, c);
}

}  // namespace

// original 0x50A760 (hidden in 0x50A020): jmp [EffectKind18Sub2A_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2A_Run(void) {
    Dispatch("EffectKind18Sub2A_Run", AddressOf(EffectKind18Sub2A_States), EffectKind18Sub2A_States_count);
}

// original 0x50A780 (state 0): +8 = (+0x3A word is 0); the placement +0x36
// (six) gives the cell; +0x3E = -(half the ground's height) less the
// placement's word; +0x30 0, +2 up; the leader within the gate: +0x30 0xC0,
// +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2A_Start(void) {
    unsigned char* s = S();
    s[8] = rv::As<records::EffectObject>(s)->z_whole == 0 ? 1 : 0;
    const U i = Place("EffectKind18Sub2A_Start", at::kSub2ACell.v, at::kSub2APlaces);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, B(::bof3::Img(at::kSub2ACell.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, B(::bof3::Img(at::kSub2ACell.v) + 2 * i + 1));
    s = S();
    const long e = Elevation(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0u - Half(e) - W(::bof3::Img(at::kSub2AHeight.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    ++s[2];
    if (Near(s, s[8] != 0)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xC0);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A8D0 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub2A_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A9B0 (state 2): +0x30 up 0x10; at 0xC0 or above +2 up. The
// draw. (State 3 is E5E's 0x508670: the same wait for the leader to leave,
// without a draw.)
extern "C" void __cdecl EffectKind18Sub2A_Open(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) + 0x10u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) >= 0xC0) ++s[2];
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A9D0 (state 4): +0x30 down 0x10; at 0 or below the sound 0x201
// and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub2A_Close(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) - 0x10u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) <= 0) Closed();
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50AA10 (states 0, 1, 2 and 4 call or jump to it): a draw mode
// committed, then the panel in two halves of one shaded POLY_FT4 each: across
// +8's axis the cell's width, along it the cell moved by +0x30 times the
// half's direction; heights as 0x29's; texture (+8 << 21) | (0x127 - k) |
// 0x25510000; each committed with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub2A_Draw(void) {
    unsigned char* s = S();
    if (s[8] != 0)
        Across(2, (rv::As<records::EffectObject>(s)->z_whole << 7) - 0x3FC0u);
    else
        Across(0, (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u);
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U page = 0x127;
    for (U k = 0; k < 2; ++k, --page) {
        unsigned char* const p = Quad();
        s = S();
        const U d = static_cast<U>(S8(B(::bof3::Img(at::kSub2ASlide.v) + k)));
        if (s[8] != 0) {
            const U a = rv::As<records::EffectWork>(s)->work_30 * d + (Word(s + 0x36) << 7) - 0x3F40u;
            Prim_VertexScratch[2].x = a;
            Prim_VertexScratch[0].x = a;
            const U b = Word(s + 0x30) * d + (Word(s + 0x36) << 7) - 0x4040u;
            Prim_VertexScratch[3].x = b;
            Prim_VertexScratch[1].x = b;
        } else {
            const U a = Word(s + 0x30) * d + (Word(s + 0x3A) << 7) - 0x3F40u;
            Prim_VertexScratch[2].y = a;
            Prim_VertexScratch[0].y = a;
            const U b = Word(s + 0x30) * d + (Word(s + 0x3A) << 7) - 0x4040u;
            Prim_VertexScratch[3].y = b;
            Prim_VertexScratch[1].y = b;
        }
        GroundLifted();
        Project(p);
        s = S();
        SH_CALL(Prim_SetTexture)((static_cast<U>(s[8]) << 21) | page | 0x25510000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x50ACB0 (hidden in 0x50AA10): jmp [EffectKind18Sub3C_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub3C_Run(void) {
    Dispatch("EffectKind18Sub3C_Run", AddressOf(EffectKind18Sub3C_States), EffectKind18Sub3C_States_count);
}

// original 0x50ACD0 (state 0): +0x30 0, +2 up; the draw, flat.
extern "C" void __cdecl EffectKind18Sub3C_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    ++S()[2];
    SH_CALL(EffectKind18Sub3C_Draw)(0);
}

// original 0x50ACF0 (state 1): story flag 0x67 set: the sound 0x200 unless
// Field_Request, +2 up; the draw on the ground.
extern "C" void __cdecl EffectKind18Sub3C_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x67) != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    SH_CALL(EffectKind18Sub3C_Draw)(1);
}

// original 0x50AD30 (state 2): +0x30 up 0x20; at 0x100 or above
// MoveCmd_TestFB(9, 0xF) (answer unread) and Effect_Release. The draw, flat
// (after the release too).
extern "C" void __cdecl EffectKind18Sub3C_Rise(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) + 0x20u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) >= 0x100) {
        SH_CALL(MoveCmd_TestFB)(9, 0xF);
        SH_CALL(Effect_Release)();
    }
    SH_CALL(EffectKind18Sub3C_Draw)(0);
}

// original 0x50AD70 (the three states call it): two quads, each a draw mode
// linked at the record's point (dy 2) and one shaded POLY_FT4 - x from +0x30
// (-0x3BC0 and -0x3AC0), z fixed (the near pair (-0x388 - k) << 4, the far
// 0xC780); `edge` set: the heights from the ground (less half its height, the
// far pair of the first quad 0x180 above); clear: fixed (-0x480, the far pair
// -0x300 for the first quad, -0x480 for the second). Textures the two dwords
// of 0x65E9E4; each linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub3C_Draw(int edge) {
    unsigned char* s = S();
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    unsigned char* const v2 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
    unsigned char* const v3 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18);
    SetWord(v2, Word(s + 0x30) - 0x3BC0u);
    SetWord(v0, Word(s + 0x30) - 0x3BC0u);
    SetWord(v3, Word(s + 0x30) - 0x3AC0u);
    SetWord(v1, Word(s + 0x30) - 0x3AC0u);
    for (U k = 0; k < 2; ++k) {
        DrawMode();
        LinkAtRecord(2, 0xC);
        unsigned char* const p = Quad();
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v3)->y, 0xC780);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v2)->y, 0xC780);
        const U near = (0xFFFFFC78u - k) << 4;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->y, near);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->y, near);
        if (edge != 0) {
            U gx = GroundOf(Word(v0)), gz = GroundOf(near);
            long e = Elevation(gx, gz);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, 0u - Half(e));
            const U t = k == 0 ? 0x180u : 0u;
            e = Elevation(gx, gz);
            gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v1)->y));
            gx = GroundOf(Word(v1));
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v2)->z, t - Half(e));
            e = Elevation(gx, gz);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, 0u - Half(e));
            e = Elevation(gx, gz);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v3)->z, t - Half(e));
        } else {
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, 0xFB80);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, 0xFB80);
            const U t = (((k == 0 ? 1u : 0u) - 3u) * 3u) << 7;
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v3)->z, t);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v2)->z, t);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(UL(::bof3::Img(at::kSub3CTextures.v) + 4 * k), p, 1);
        LinkAtRecord(2, 0x48);
    }
}

void EffectKind18Sub2a_Inject() {
    BOF3_INJECT(EffectKind18Sub2A_Run);
    BOF3_INJECT(EffectKind18Sub2A_Start);
    BOF3_INJECT(EffectKind18Sub2A_WaitNear);
    BOF3_INJECT(EffectKind18Sub2A_Open);
    BOF3_INJECT(EffectKind18Sub2A_Close);
    BOF3_INJECT(EffectKind18Sub2A_Draw);
    BOF3_INJECT(EffectKind18Sub3C_Run);
    BOF3_INJECT(EffectKind18Sub3C_Start);
    BOF3_INJECT(EffectKind18Sub3C_WaitFlag);
    BOF3_INJECT(EffectKind18Sub3C_Rise);
    BOF3_INJECT(EffectKind18Sub3C_Draw);
}

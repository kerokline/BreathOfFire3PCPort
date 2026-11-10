// Moved here by tools/move_funcs.py from effect_5f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_29.h"

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

bool Far(const unsigned char* s, bool z_axis) { return !Within(s, z_axis, 0x20000, 0x20000); }

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

// 0x509CB0's and 0x50A2F0's common start: +8 = (+0x3A word is 0); the
// placement +0x36 (four) gives the cell; +0x3E = -(half the ground's height
// at the record's point) less the placement's word; +0x20 its texture; +0x30
// 0, +2 up.
void Start29(const char* who) {
    unsigned char* s = S();
    s[8] = rv::As<records::EffectObject>(s)->z_whole == 0 ? 1 : 0;
    const U i = Place(who, at::kSub29Cell.v, at::kSub29Places);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, B(::bof3::Img(at::kSub29Cell.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, B(::bof3::Img(at::kSub29Cell.v) + 2 * i + 1));
    s = S();
    const long e = Elevation(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0u - Half(e) - W(::bof3::Img(at::kSub29Height.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, UL(::bof3::Img(at::kSub29Texture.v) + 4 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    ++s[2];
}

}  // namespace

// original 0x509C90 (hidden in 0x509C00): jmp [EffectKind18Sub29_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub29_Run(void) {
    Dispatch("EffectKind18Sub29_Run", AddressOf(EffectKind18Sub29_States), EffectKind18Sub29_States_count);
}

// original 0x509CB0 (state 0 of 0x29 and of 0x49): the start; the leader
// within the gate: +0x30 0x80, +2 = 3 (open at once). The draw.
extern "C" void __cdecl EffectKind18Sub29_Start(void) {
    Start29("EffectKind18Sub29_Start");
    unsigned char* const s = S();
    if (Near(s, s[8] != 0)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0x80);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509E10 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509EF0 (state 2 of 0x29, 0x48 and 0x49): +0x30 up 0x10; at 0x80
// or above +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_Open(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) + 0x10u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) >= 0x80) ++s[2];
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509F10 (state 3): the leader two cells or more away: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, s[8] != 0)) ++s[2];
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509FE0 (state 4 of 0x29 and 0x49): +0x30 down 0x10; at 0 or
// below the sound 0x201 and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub29_Close(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) - 0x10u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) <= 0) Closed();
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A020 (the three sub-kinds' states call or jump to it; state 3 of
// 0x48 is it): the panel in two halves, each a draw mode linked at the
// record's point (dy -1) and one shaded POLY_FT4. Across +8's axis the cell's
// width; along it half k (0, 1) at the cell + k (z - k when +8 is clear)
// moved by +0x30 times the half's direction; every corner less half the
// ground's height and +0x3E, two 0x180 lower; texture (+0x20 + k) | +8 << 21;
// linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub29_Draw(void) {
    unsigned char* s = S();
    if (s[8] != 0)
        Across(2, (rv::As<records::EffectObject>(s)->z_whole << 7) - 0x3FC0u);
    else
        Across(0, (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u);
    for (U k = 0; k < 2; ++k) {
        DrawMode();
        LinkAtRecord(-1, 0xC);
        unsigned char* const p = Quad();
        s = S();
        const U d = static_cast<U>(S8(B(::bof3::Img(at::kSub29Slide.v) + k)));
        if (s[8] != 0) {
            const U a = ((k + rv::As<records::EffectObject>(s)->x_whole) << 7) - rv::As<records::EffectWork>(s)->work_30 * d - 0x4040u;
            Prim_VertexScratch[2].x = a;
            Prim_VertexScratch[0].x = a;
            const U b = ((k + rv::As<records::EffectObject>(s)->x_whole) << 7) - rv::As<records::EffectWork>(s)->work_30 * d - 0x3FC0u;
            Prim_VertexScratch[3].x = b;
            Prim_VertexScratch[1].x = b;
        } else {
            const U a = ((rv::As<records::EffectObject>(s)->z_whole - k) << 7) + rv::As<records::EffectWork>(s)->work_30 * d - 0x3F40u;
            Prim_VertexScratch[2].y = a;
            Prim_VertexScratch[0].y = a;
            const U b = ((rv::As<records::EffectObject>(s)->z_whole - k) << 7) + rv::As<records::EffectWork>(s)->work_30 * d - 0x3FC0u;
            Prim_VertexScratch[3].y = b;
            Prim_VertexScratch[1].y = b;
        }
        GroundLifted();
        Project(p);
        s = S();
        SH_CALL(Prim_SetTexture)((static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) + k) | (static_cast<U>(s[8]) << 21), p, 1);
        LinkAtRecord(-1, 0x48);
    }
}

// original 0x50A2D0 (hidden in 0x50A020): jmp [EffectKind18Sub48_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub48_Run(void) {
    Dispatch("EffectKind18Sub48_Run", AddressOf(EffectKind18Sub48_States), EffectKind18Sub48_States_count);
}

// original 0x50A2F0 (state 0): 0x29's start without the leader's test. The draw.
extern "C" void __cdecl EffectKind18Sub48_Start(void) {
    Start29("EffectKind18Sub48_Start");
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A390 (state 1): Cond_ByteFE 0x10: the sound 0x200 unless
// Field_Request, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub48_WaitCue(void) {
    if (Cond_ByteFE == 0x10) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A3C0 (hidden in 0x50A020): jmp [EffectKind18Sub49_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub49_Run(void) {
    Dispatch("EffectKind18Sub49_Run", AddressOf(EffectKind18Sub49_States), EffectKind18Sub49_States_count);
}

// original 0x50A3E0 (state 1): Cond_ByteFE 0x20: the sound 0x200 unless
// Field_Request, +2 up; then, story flag 0x8F set and the leader within the
// gate (along x): the sound again, +2 up again. The draw.
extern "C" void __cdecl EffectKind18Sub49_WaitCue(void) {
    if (Cond_ByteFE == 0x20) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x8F) != 0) StepIfNear(false);
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A4A0 (state 3): unless Cond_ByteFE is 0x20, the leader two
// cells or more away (along x): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub49_WaitFar(void) {
    if (Cond_ByteFE != 0x20) {
        unsigned char* const s = S();
        if (Far(s, false)) ++s[2];
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A510 (hidden in 0x50A020; PSX twin 0x801F38F0): v = +0xB less
// 0x4B. +2 at 0 (the first frame): +0x10 = (Rand & 0xFF) + ((3 v + 6) << 9),
// +0x2E / +0x30 from the pair (+0x34's low word is 0) picks, +0x3A up by
// Rand & 7, +2 up. Then, on Draw_PassFlags bit 2 only: +0x38 += +0x10 (the
// drift); +0x3A past +0x30 (signed) back to +0x2E; a draw mode committed and
// three shaded POLY_FT4s 0xA00 apart in x and 0x180 in z from the point's
// (>> 9) corner, half-width b = |0xF - (Frame_Counter & 0x1F)| + ((2 - v) << 8),
// height 0xB00, texture v | 0xBB509100, each committed with 0x48 bytes.
//
// The original's far corners (v1, v3) take their x from a stack word it never
// writes, and so do every corner of the second and third quads (the register
// that held the near x is reloaded from it): a latent defect (docs/effect_5f.md
// section 6). Ours takes the near corners' x there, the quad's own: x0 + a + b
// for v1 and v3, x0 + a - b for v0 and v2 of every quad.
extern "C" void __cdecl EffectKind18Sub4B_Run(void) {
    unsigned char* s = S();
    const U v = static_cast<U>(s[0xB]) - 0x4Bu;
    if (s[2] == 0) {
        const U r = static_cast<U>(SH_CALL(Rand)());
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, (r & 0xFF) + ((v * 3 + 6) << 9));
        const U pick = rv::As<records::EffectObject>(s)->x_frac == 0 ? 1u : 0u;
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, B(::bof3::Img(at::kSub4BBounds.v) + 2 * pick));
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, B(::bof3::Img(at::kSub4BBounds.v) + 2 * pick + 1));
        unsigned char* const z = S() + 0x3A;
        const U r2 = static_cast<U>(SH_CALL(Rand)());
        SetWord(z, Word(z) + (r2 & 7));
        s = S();
        ++s[2];
    }
    if ((Draw_PassFlags & 4) == 0) return;
    rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + rv::As<records::EffectWork>(s)->work_10;
    if (S16(rv::As<records::EffectObject>(s)->z_whole) > S16(rv::As<records::EffectWork>(s)->work_30)) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, rv::As<records::EffectWork>(s)->work_2e);
    const U pulse = Frame_Counter & 0x1F;
    const U z0 = static_cast<U>(rv::As<records::EffectObject>(s)->z >> 9) - 0x3FC0u;
    const U b = static_cast<U>(Abs(0xFu - pulse)) + ((2u - v) << 8);
    const U x0 = static_cast<U>(rv::As<records::EffectObject>(s)->x >> 9) - 0x4EC0u;
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    const U texture = v | 0xBB509100u;
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    unsigned char* const v2 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
    unsigned char* const v3 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18);
    for (U k = 0; k < 3; ++k) {
        unsigned char* const p = Quad();
        const U a = k * 0xA00, c = k * 0x180;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, 0xB00);
        SetWord(v0, x0 + a - b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->y, c - b + z0);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->y, c - b + z0);
        SetWord(v2, x0 + a - b);
        SetWord(v1, x0 + a + b);
        SetWord(v3, x0 + a + b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v2)->y, c + z0 + b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v3)->y, c + z0 + b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v2)->z, 0xB00);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v3)->z, 0xB00);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, 0xB00);
        Project(p);
        SH_CALL(Prim_SetTexture)(texture, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

void EffectKind18Sub29_Inject() {
    BOF3_INJECT(EffectKind18Sub29_Run);
    BOF3_INJECT(EffectKind18Sub29_Start);
    BOF3_INJECT(EffectKind18Sub29_WaitNear);
    BOF3_INJECT(EffectKind18Sub29_Open);
    BOF3_INJECT(EffectKind18Sub29_WaitFar);
    BOF3_INJECT(EffectKind18Sub29_Close);
    BOF3_INJECT(EffectKind18Sub29_Draw);
    BOF3_INJECT(EffectKind18Sub48_Run);
    BOF3_INJECT(EffectKind18Sub48_Start);
    BOF3_INJECT(EffectKind18Sub48_WaitCue);
    BOF3_INJECT(EffectKind18Sub49_Run);
    BOF3_INJECT(EffectKind18Sub49_WaitCue);
    BOF3_INJECT(EffectKind18Sub49_WaitFar);
    BOF3_INJECT(EffectKind18Sub4B_Run);
}
